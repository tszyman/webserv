#include "network/EventLoop.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"
#include "http/HttpErrorPage.hpp"
#include "http/HttpResponse.hpp"

EventLoop::EventLoop(const std::vector<SocketEngine*>& engines) : _is_running(true), _server_engines(engines)
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

void EventLoop::run()
{
	Logger::info("Starting the minimal event loop...");

	while (_is_running)
	{
		int ready_count = _poller.poll(-1);

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
				Connection* new_conn = active_engine->acceptConnection();
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
				conn->getParser().feed(buffer, bytes_read);
				RequestParser::ParserState state = conn->getParser().getState();

				if (state == RequestParser::STATE_COMPLETE)
				{
					Logger::info("Request fully parsed! Method: " + conn->getParser().getMethod() + " Path: " + conn->getParser().getPath());

					HttpResponse response;
					std::string method = conn->getParser().getMethod();
					std::string path = conn->getParser().getPath();

					if (method == "GET")
					{
						if (path == "/") path = "index.html";

						std::string local_path = "www" + path;

						response.serveStaticFile(local_path);
					}
					else
					{
						ErrorPage::tryBuildDefault(405, response);
					}

					conn->appendResponse(response.toString());
					_poller.setEvents(current_fd, POLLIN | POLLOUT);
				}
				else if (state == RequestParser::STATE_ERROR)
				{
					Logger::warning("Request parsing error on FD: " + StringUtils::to_string(current_fd));

					HttpResponse response;
					ErrorPage::tryBuildDefault(400, response);

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
					_poller.setEvents(current_fd, POLLIN);
				}
			}
		}
	}
	}
	}
	}
