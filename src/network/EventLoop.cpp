#include "network/EventLoop.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"

EventLoop::EventLoop(SocketEngine* engine) : _server_engine(engine), _is_running(true)
{
	if(!_server_engine)
		throw std::runtime_error("EventLoop initialized with NULL SocketEngine");
	_poller.addFd(_server_engine->getFd(), POLLIN);
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

		// 1. IO Multiplexing Safeguard: Detect errors and broken connections
		if (revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			Logger::warning(std::string("Socket error/hangup detected on FD: ") +StringUtils::to_string(current_fd));
		
			// If new client socket, clean up memory
			if (current_fd != _server_engine->getFd())
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
			// A: Event on the listening socket (New Client)
			if (current_fd == _server_engine->getFd())
			{
				Connection* new_conn = _server_engine->acceptConnection();
				if (new_conn)
				{
					int client_fd = new_conn->getFd();
					_connections[client_fd] = new_conn;
					_poller.addFd(client_fd, POLLIN);

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
				buffer[bytes_read] = '\0';

				Logger::debug(std::string("Received ") + StringUtils::to_string(bytes_read) + " bytes from FD " + StringUtils::to_string(current_fd));
				std::string mock_http_response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello Webserv";
				_connections[current_fd]->appendResponse(mock_http_response);

				_poller.setEvents(current_fd, POLLIN | POLLOUT);
			}
		}
	}

	if (revents & POLLOUT)
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
