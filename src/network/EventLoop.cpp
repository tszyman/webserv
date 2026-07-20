#include "network/EventLoop.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"
#include "http/HttpErrorPage.hpp"
#include "http/HttpResponse.hpp"
#include <sys/wait.h>
#include <cerrno>

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
	for (std::map<int, CgiSession*>::iterator it = _cgiByClient.begin(); it != _cgiByClient.end(); ++it)
		destroyCgi(it->second);
	_cgiByClient.clear();
	_cgiByFd.clear();
	for(std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it)
		delete it->second;
	_connections.clear(); 
}

void EventLoop::startCgi(Connection *connection, const RequestParser& request, const std::string& scriptPath, const std::string& executable)
{
	CgiSession *session = new CgiSession();
	session->clientFd = connection->getFd();
	session->handler = new CgiHandler();
	const std::vector<char>& body = request.getBody();
	session->input.assign(body.begin(), body.end());
	if (!session->handler->init(request, scriptPath, executable))
	{
		delete session->handler;
		delete session;
		HttpResponse response;
		ErrorPage::tryBuildDefault(500, response);
		connection->appendResponse(response.toString());
		_poller.setEvents(connection->getFd(), POLLIN | POLLOUT);
		return;
	}
	const int writeFd = session->handler->getWriteFd();
	const int readFd = session->handler->getReadFd();
	_cgiByClient[session->clientFd] = session;
	_cgiByFd[writeFd] = session;
	_cgiByFd[readFd] = session;
	_poller.addFd(readFd, POLLIN);
	if (session->input.empty())
	{
		close(writeFd);
		_poller.removeFd(writeFd);
		_cgiByFd.erase(writeFd);
		session->writeClosed = true;
	}
	else
		_poller.addFd(writeFd, POLLOUT);
	_poller.setEvents(session->clientFd, 0);
}

void EventLoop::destroyCgi(CgiSession *session)
{
	if (session == NULL) return;
	const int writeFd = session->handler->getWriteFd();
	const int readFd = session->handler->getReadFd();
	std::map<int, CgiSession*>::iterator writeIt = _cgiByFd.find(writeFd);
	if (writeIt != _cgiByFd.end() && writeIt->second == session) { _poller.removeFd(writeFd); close(writeFd); _cgiByFd.erase(writeIt); }
	std::map<int, CgiSession*>::iterator readIt = _cgiByFd.find(readFd);
	if (readIt != _cgiByFd.end() && readIt->second == session) { _poller.removeFd(readFd); close(readFd); _cgiByFd.erase(readIt); }
	_cgiByClient.erase(session->clientFd);
	delete session->handler;
	delete session;
}

void EventLoop::finishCgi(CgiSession *session)
{
	if (session == NULL || !session->readClosed) return;
	std::map<int, Connection*>::iterator client = _connections.find(session->clientFd);
	if (client != _connections.end())
	{
		int statusCode = 200;
		std::string body = session->output;
		HttpResponse response;
		const std::string::size_type separator = session->output.find("\r\n\r\n");
		if (separator != std::string::npos)
		{
			const std::string headers = session->output.substr(0, separator);
			body = session->output.substr(separator + 4);
			std::string::size_type lineStart = 0;
			while (lineStart < headers.size())
			{
				const std::string::size_type lineEnd = headers.find("\r\n", lineStart);
				const std::string line = headers.substr(lineStart, lineEnd - lineStart);
				const std::string::size_type colon = line.find(':');
				if (colon != std::string::npos)
				{
					const std::string key = line.substr(0, colon);
					const std::string value = line.substr(colon + 1);
					if (key == "Status") statusCode = std::atoi(value.c_str());
					else if (key != "Content-Length") response.setHeader(key, value);
				}
				if (lineEnd == std::string::npos) break;
				lineStart = lineEnd + 2;
			}
		}
		response.setStatusCode(statusCode);
		response.setBody(body);
		response.setHeader("Content-Length", HttpResponse::numberToString(body.size()));
		client->second->appendResponse(response.toString());
		_poller.setEvents(session->clientFd, POLLIN | POLLOUT);
	}
	if (session->handler->getPid() > 0) waitpid(session->handler->getPid(), NULL, WNOHANG);
	destroyCgi(session);
}

void EventLoop::handleCgiFd(int fd, short revents)
{
	std::map<int, CgiSession*>::iterator found = _cgiByFd.find(fd);
	if (found == _cgiByFd.end()) return;
	CgiSession *session = found->second;
	if (fd == session->handler->getWriteFd() && (revents & POLLOUT))
	{
		const ssize_t sent = write(fd, session->input.data() + session->inputOffset, session->input.size() - session->inputOffset);
		if (sent > 0) session->inputOffset += static_cast<size_t>(sent);
		if (sent < 0 || session->inputOffset == session->input.size())
		{
			_poller.removeFd(fd); close(fd); _cgiByFd.erase(fd); session->writeClosed = true;
		}
	}
	if (fd == session->handler->getReadFd() && (revents & (POLLIN | POLLHUP)))
	{
		char buffer[4096];
		ssize_t count = read(fd, buffer, sizeof(buffer));
		while (count > 0)
		{
			session->output.append(buffer, static_cast<size_t>(count));
			count = read(fd, buffer, sizeof(buffer));
		}
		if (count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
		{
			_poller.removeFd(fd); close(fd); _cgiByFd.erase(fd); session->readClosed = true;
			finishCgi(session);
		}
	}
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
		if (_cgiByFd.find(current_fd) != _cgiByFd.end())
		{
			handleCgiFd(current_fd, revents);
			continue;
		}

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
				Connection* new_conn = active_engine->acceptConnection(0);
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
						Router router(current_config->clientMaxBodySize);
						for (size_t i = 0; i < current_config->locations.size(); ++i)
						{
							router.addLocation(current_config->locations[i]);
						}

						std::string scriptPath;
						std::string executable;
						if (router.getCgiTarget(conn->getParser(), scriptPath, executable))
						{
							startCgi(conn, conn->getParser(), scriptPath, executable);
							continue;
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
					else if (state == RequestParser::STATE_ERROR
						|| (state == RequestParser::STATE_PAYLOAD_TOO_LARGE && conn->getParser().isOversizedBodyDrained()))
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
