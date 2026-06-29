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

	for (int i = fds.size() - 1; i >= 0; --i)
	{
		if (fds[i].revents & POLLIN)
		{
			int current_fd = fds[i].fd;
		
		// First case. New Client
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

		//Second case. Data or Disconnect from Client socket.

		else
		{
			char buffer[1024];
			ssize_t bytes_read = recv(current_fd, buffer, sizeof(buffer) - 1, 0);

			if (bytes_read <= 0)
			{
				Logger::info(std::string("Client disconnected or error on FD: ") + StringUtils::to_string(current_fd));

				delete _connections[current_fd];
				_connections.erase(current_fd);
				_poller.removeFd(current_fd);
			}

			else
			{
				buffer[bytes_read] = '\0';

				Logger::debug(std::string("Received ") + StringUtils::to_string(bytes_read) + " bytes from FD " + StringUtils::to_string(current_fd));
				Logger::debug(std::string("--- Data snippet ---\n") + buffer + "\n--------------------");
			}
		}
	}
}
	}
	}
