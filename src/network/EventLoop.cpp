#include "network/EventLoop.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"
#include "http/HttpErrorPage.hpp"
#include "http/HttpResponse.hpp"

EventLoop::EventLoop(const std::vector<SocketEngine*>& engines, const std::vector<ServerConfig>& servers)
	: _is_running(true), _server_engines(engines), _servers(servers)
{
	if(_server_engines.empty())
		throw std::runtime_error("EventLoop initialized with empty SocketEngine list");

	for (size_t i = 0; i < _server_engines.size(); ++i)
		_poller.addFd(_server_engines[i]->getFd(), POLLIN);
}

EventLoop::~EventLoop()
{
	for(std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it)
		delete it->second;
	_connections.clear(); 
}

size_t EventLoop::getMaxBodySizeForPort(int port) const
{
	for (size_t i = 0; i < _servers.size(); ++i)
	{
		if (_servers[i].port == port)
			return _servers[i].clientMaxBodySize;
	}

	return 1048576;
}

const ServerConfig* EventLoop::matchServerConfig(const std::string& hostHeader) const
{
	std::string host = hostHeader;

	size_t colon_pos = host.find(':');
	if (colon_pos != std::string::npos)
		host = host.substr(0, colon_pos);

	for (size_t i = 0; i < _servers.size(); ++i)
	{
		if (_servers[i].serverName == host)
			return &_servers[i];
	}

	if (!_servers.empty())
		return &_servers[0];
	
	return NULL;
}

void EventLoop::run()
{
	Logger::info("Starting the minimal event loop...");
	const int TIMEOUT_LIMIT = 15;

	while (_is_running)
	{
		std::map<int, Connection*>::iterator it = _connections.begin();
		while (it != _connections.end())
		{
			if (it->second->isTimedOut(TIMEOUT_LIMIT))
			{
				int timeout_fd = it->first;
				Logger::warning("Connection timed out. Closing FD " + StringUtils::to_string(timeout_fd));
				close(timeout_fd);
				_poller.removeFd(timeout_fd);
				delete it->second;
				_connections.erase(it++);
			}
			else
			{
				++it;
			}
		}

		// Keep queued responses responsive even when no additional socket event arrives.
		int ready_count = _poller.poll(100);

		if (ready_count < 0)
		{
			Logger::error("poll() failed. Exiting loop.");
			break;
		}

	std::vector<struct pollfd>& fds = _poller.getFds();

	//iterate backwards because removeFd() modifies the vector during execution
	for (int i = fds.size() - 1; i >= 0; --i)
	{
		int current_fd = fds[i].fd;
		short revents = fds[i].revents;

		bool is_server_socket = false;
		SocketEngine* active_engine = NULL;

		for (size_t j = 0; j < _server_engines.size(); ++j)
		{
			if (current_fd == _server_engines[j]->getFd())
			{
				is_server_socket = true;
				active_engine = _server_engines[j];
				break;
			}
		}

		// 1. IO Multiplexing Safeguard: Detect errors and broken connections
		if (revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			Logger::warning(std::string("Socket error/hangup detected on FD: ") +StringUtils::to_string(current_fd));
		
			// If new client socket, clean up memory
			if (!is_server_socket)
			{
				delete _connections[current_fd];
				_connections.erase(current_fd);
			}
			//remove the broken socket from our poller
			_poller.removeFd(current_fd);
			continue; // skip to next FD
		}
		// 2. Main read handling with POLLIN
		if (revents & POLLIN)
		{
			if (is_server_socket)
			{
				size_t max_body_size = getMaxBodySizeForPort(active_engine->getPort());
				Connection* new_conn = active_engine->acceptConnection(max_body_size);
				if (new_conn != NULL)
				{
					int client_fd = new_conn->getFd();
					_poller.addFd(client_fd, POLLIN);
					_connections[client_fd] = new_conn;

					Logger::info(std::string("Accepted and monitoring new client FD: ") + StringUtils::to_string(client_fd));
				}
			}
		// B: Event on a client socket (Incoming data).
		else
		{
			char buffer[1024];
			ssize_t bytes_read = recv(current_fd, buffer, sizeof(buffer) - 1, 0);

			// Graceful disconnection
			if (bytes_read == 0)
			{
				Logger::info(std::string("Client gracefully disconnected on FD: ") + StringUtils::to_string(current_fd));
				delete _connections[current_fd];
				_connections.erase(current_fd);
				_poller.removeFd(current_fd);
			}
			//Read error (return -1)
			else if (bytes_read < 0)
			{
				Logger::warning(std::string("recv() error on FD: ") + StringUtils::to_string(current_fd));
				delete _connections[current_fd];
				_connections.erase(current_fd);
				_poller.removeFd(current_fd);
			}
			// Successful data read
			else
			{
				Connection* conn = _connections[current_fd];
				conn->updateLastActivity();
				conn->getParser().feed(buffer, bytes_read);
				RequestParser::ParserState state = conn->getParser().getState();

				if (state == RequestParser::STATE_COMPLETE)
				{
					Logger::info("Request fully parsed! Path: " + conn->getParser().getPath());

					HttpResponse response;
					const std::map<std::string, std::string>& headers = conn->getParser().getHeaders();
					std::string host = "";
					std::map<std::string, std::string>::const_iterator it = headers.find("Host");
					if (it != headers.end())
						host = it->second;
					
					const ServerConfig* current_config = matchServerConfig(host);

					if (conn->getParser().getVersion() == "HTTP/1.1" && host.empty())
					{
						ErrorPage::tryBuildDefault(400, response);
					}
					else if (current_config != NULL)
					{
						Router router;
						for (size_t i = 0; i < current_config->locations.size(); ++i)
						{
							router.addLocation(current_config->locations[i]);
						}

						router.route(conn->getParser(), response);
					}
					else
					{
						ErrorPage::tryBuildDefault(500, response);
					}

					conn->appendResponse(response.toString());
					_poller.setEvents(current_fd, POLLIN | POLLOUT);
				}
					else if (state == RequestParser::STATE_ERROR || state == RequestParser::STATE_PAYLOAD_TOO_LARGE)
				{
					Logger::warning("Request parsing error on FD: " + StringUtils::to_string(current_fd));

					HttpResponse response;
						ErrorPage::tryBuildDefault(state == RequestParser::STATE_PAYLOAD_TOO_LARGE ? 413 : 400, response);

					conn->appendResponse(response.toString());
					_poller.setEvents(current_fd, POLLIN | POLLOUT);
				}
			}
		}
	}

	if (revents & POLLOUT)
	{
		if(_connections.find(current_fd) != _connections.end())
		{
			Connection* conn = _connections[current_fd];
			std::string& response = conn->getResponseBuffer();

			if (!response.empty())
			{
				ssize_t bytes_sent = send(current_fd, response.c_str(), response.size(), 0);

				if (bytes_sent > 0)
				{
					Logger::debug(std::string("Sent ") + StringUtils::to_string(bytes_sent) + " bytes to FD " + StringUtils::to_string(current_fd));
					conn->eraseSentData(bytes_sent);
				}
				else if (bytes_sent < 0)
				{
					Logger::error(std::string("Send() error on FD: ") + StringUtils::to_string(current_fd));
				}

				if(conn->getResponseBuffer().empty())
				{
					const std::map<std::string, std::string>& headers = conn->getParser().getHeaders();
					// A malformed or oversized request cannot safely be reused as a
					// persistent connection: parsing may have stopped before all
					// headers (including Connection) were read.
					bool keep_alive = conn->getParser().getState() != RequestParser::STATE_ERROR
						&& conn->getParser().getState() != RequestParser::STATE_PAYLOAD_TOO_LARGE;

					std::map<std::string, std::string>::const_iterator it = headers.find("Connection");
					if (it != headers.end() && it->second == "close")
					{
						keep_alive = false;
					}

					if (keep_alive)
					{
						Logger::debug("Keep-Alive: Reseting state for FD " + StringUtils::to_string(current_fd));
						conn->reset();
						_poller.setEvents(current_fd, POLLIN);
					}
					else
					{
						if (conn->getParser().getState() == RequestParser::STATE_ERROR
							|| conn->getParser().getState() == RequestParser::STATE_PAYLOAD_TOO_LARGE)
						{
							// The client may still have request bytes in flight.  Half-close
							// our write side after the error response so they receive it
							// instead of a TCP reset caused by unread inbound data.
							shutdown(current_fd, SHUT_WR);
							_poller.setEvents(current_fd, POLLIN);
						}
						else
						{
							Logger::info("Connection: close requested. Closing FD " + StringUtils::to_string(current_fd));
							close(current_fd);
							delete conn;
							_connections.erase(current_fd);
							_poller.removeFd(current_fd);
						}
					}
				}
			}
		}
	}
	}
	}
	}
