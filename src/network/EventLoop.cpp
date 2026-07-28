#include "network/EventLoop.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"
#include "http/HttpErrorPage.hpp"
#include "http/HttpResponse.hpp"
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

// Set to 0 to stop printing complete HTTP responses during debugging.
#define WEBSERV_LOG_RESPONSES 0

static std::string trimTrailingCrlf(const std::string& line)
{
	std::string result = line;
	if (!result.empty() && result[result.size() - 1] == '\r')
		result.erase(result.size() - 1);
	return result;
}

bool EventLoop::parseCgiOutput(const std::string& rawOutput, HttpResponse& response)
{
	std::string::size_type separator = rawOutput.find("\r\n\r\n");
	std::string::size_type separatorLength = 4;
	if (separator == std::string::npos)
	{
		separator = rawOutput.find("\n\n");
		separatorLength = 2;
	}

	std::string headerBlock;
	std::string body;
	if (separator == std::string::npos)
		body = rawOutput;
	else
	{
		headerBlock = rawOutput.substr(0, separator);
		body = rawOutput.substr(separator + separatorLength);
	}

	int statusCode = 200;
	bool hasContentType = false;
	std::string::size_type lineStart = 0;
	while (lineStart < headerBlock.size())
	{
		std::string::size_type lineEnd = headerBlock.find('\n', lineStart);
		std::string line;
		if (lineEnd == std::string::npos)
			line = headerBlock.substr(lineStart);
		else
			line = headerBlock.substr(lineStart, lineEnd - lineStart);
		line = trimTrailingCrlf(line);
		if (!line.empty())
		{
			std::string::size_type colon = line.find(':');
			if (colon != std::string::npos)
			{
				std::string key = line.substr(0, colon);
				std::string value = line.substr(colon + 1);
				while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
					value.erase(0, 1);
				if (key == "Status")
					statusCode = std::atoi(value.c_str());
				else if (key == "Content-Type")
				{
					response.setHeader("Content-Type", value);
					hasContentType = true;
				}
				else
					response.setHeader(key, value);
			}
		}
		if (lineEnd == std::string::npos)
			break;
		lineStart = lineEnd + 1;
	}

	response.setStatusCode(statusCode);
	response.setBody(body);
	if (!hasContentType)
		response.setHeader("Content-Type", "text/plain");
	return true;
}

bool EventLoop::is_running = true;

EventLoop::EventLoop(const std::vector<SocketEngine*>& engines, const std::vector<ServerConfig>& servers)
	: _server_engines(engines), _servers(servers)
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

size_t EventLoop::getMaxBodySizeForEndpoint(const std::string& host, int port) const
{
	size_t maxBodySize = 0;
	bool hasUnlimited = false;

	for (size_t i = 0; i < _servers.size(); ++i)
	{
		if (_servers[i].host == host && _servers[i].port == port)
		{
			if (_servers[i].clientMaxBodySize == 0)
				hasUnlimited = true;
			else if (_servers[i].clientMaxBodySize > maxBodySize)
				maxBodySize = _servers[i].clientMaxBodySize;

			for (size_t j = 0; j < _servers[i].locations.size(); ++j)
			{
				const size_t locationLimit = _servers[i].locations[j].getClientMaxBodySize();
				if (locationLimit == 0)
					hasUnlimited = true;
				else if (locationLimit > maxBodySize)
					maxBodySize = locationLimit;
			}
		}
	}

	if (hasUnlimited)
		return 0;
	if (maxBodySize == 0)
		return 1048576;
	return maxBodySize;
}

const ServerConfig* EventLoop::matchServerConfig(const std::string& hostHeader,
	const std::string& listeningHost, int listeningPort) const
{
	std::string host = hostHeader;

	size_t colon_pos = host.find(':');
	if (colon_pos != std::string::npos)
		host = host.substr(0, colon_pos);

	for (size_t i = 0; i < _servers.size(); ++i)
	{
		if (_servers[i].host == listeningHost && _servers[i].port == listeningPort
			&& _servers[i].serverName == host)
			return &_servers[i];
	}

	for (size_t i = 0; i < _servers.size(); ++i)
		if (_servers[i].host == listeningHost && _servers[i].port == listeningPort)
			return &_servers[i];
	
	return NULL;
}

void EventLoop::queueResponse(Connection* connection, const HttpResponse& response)
{
	const std::string rawResponse = response.toString();

#if WEBSERV_LOG_RESPONSES
	Logger::debug("----- HTTP response for FD "
		+ StringUtils::to_string(connection->getFd())
		+ " -----\n" + rawResponse + "\n----- end HTTP response -----");
#endif

	connection->appendResponse(rawResponse);
	connection->setCloseAfterResponse(response.shouldCloseConnection());
	_poller.setEvents(connection->getFd(), POLLIN | POLLOUT);
}

void EventLoop::run()
{
	Logger::info("Starting the minimal event loop...");
	const int TIMEOUT_LIMIT = 15;

	while (EventLoop::is_running)
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
			if (!EventLoop::is_running)
			{
				Logger::info("Event loop terminated gracefully.");
				break;
			}
			Logger::error("poll() failed. Exiting loop.");
			break;
		}

	std::vector<struct pollfd>& fds = _poller.getFds();

	//iterate backwards because removeFd() modifies the vector during execution
	for (int i = fds.size() - 1; i >= 0; --i)
	{
		int current_fd = fds[i].fd;
		short revents = fds[i].revents;
		bool is_cgi_read_fd = _cgi_states.find(current_fd) != _cgi_states.end();
		bool is_cgi_write_fd = _cgi_write_to_read.find(current_fd) != _cgi_write_to_read.end();

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
		if ((revents & (POLLERR | POLLNVAL | POLLHUP))
			&& !is_cgi_read_fd && !is_cgi_write_fd)
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
		if ((revents & POLLIN) || (is_cgi_read_fd && (revents & POLLHUP)))
		{
			if (is_server_socket)
			{
				size_t max_body_size = getMaxBodySizeForEndpoint(active_engine->getHost(), active_engine->getPort());
				Connection* new_conn = active_engine->acceptConnection(max_body_size);
				if (new_conn != NULL)
				{
					int client_fd = new_conn->getFd();
					_poller.addFd(client_fd, POLLIN);
					_connections[client_fd] = new_conn;

					Logger::info(std::string("Accepted and monitoring new client FD: ") + StringUtils::to_string(client_fd));
				}
			}
			else if (_cgi_states.find(current_fd) != _cgi_states.end())
			{
				std::map<int, CgiState>::iterator cgiIt = _cgi_states.find(current_fd);
				if (cgiIt != _cgi_states.end())
				{
					CgiState& state = cgiIt->second;
					char cgi_buffer[4096];
					bool eof = false;
					while (true)
					{
						ssize_t bytes_read = read(current_fd, cgi_buffer, sizeof(cgi_buffer));
						if (bytes_read > 0)
						{
							state.output.append(cgi_buffer, bytes_read);
							continue;
						}
						if (bytes_read == 0)
						{
							eof = true;
							break;
						}
						if (errno == EAGAIN || errno == EWOULDBLOCK)
							break;

						Logger::warning("CGI stdout read failed on FD: " + StringUtils::to_string(current_fd));
						Connection* client_conn = state.client;
						HttpResponse errorResponse;
						ErrorPage::tryBuildDefault(500, errorResponse);
						queueResponse(client_conn, errorResponse);
						if (!state.requestBodyClosed)
						{
							close(state.writeFd);
							_poller.removeFd(state.writeFd);
							_cgi_write_to_read.erase(state.writeFd);
							state.requestBodyClosed = true;
						}
						close(current_fd);
						_poller.removeFd(current_fd);
						_cgi_states.erase(cgiIt);
						if (_connections.find(client_conn->getFd()) != _connections.end())
							_poller.setEvents(client_conn->getFd(), POLLIN | POLLOUT);
						eof = false;
						break;
					}

					if (eof)
					{
						Logger::info("CGI finished on FD: " + StringUtils::to_string(current_fd));
						Connection* client_conn = state.client;
						if (!state.requestBodyClosed)
						{
							close(state.writeFd);
							_poller.removeFd(state.writeFd);
							_cgi_write_to_read.erase(state.writeFd);
							state.requestBodyClosed = true;
						}
						int status = 0;
						waitpid(state.pid, &status, 0);
						HttpResponse response;
						parseCgiOutput(state.output, response);
						queueResponse(client_conn, response);
						close(current_fd);
						_poller.removeFd(current_fd);
						_cgi_states.erase(cgiIt);
						if (_connections.find(client_conn->getFd()) != _connections.end())
							_poller.setEvents(client_conn->getFd(), POLLIN | POLLOUT);
					}
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
				RequestParser::ParseState parseState = conn->getParser().getParseState();
				RequestParser::ParserState state = conn->getParser().getState();

				if (parseState == RequestParser::PARSE_SUCCESS)
				{
					Logger::info("Request fully parsed! Path: " + conn->getParser().getPath());

					HttpResponse response;
					const std::map<std::string, std::string>& headers = conn->getParser().getHeaders();
					std::string host = "";
					std::map<std::string, std::string>::const_iterator it = headers.find("host");
					if (it != headers.end())
						host = it->second;
					
					const ServerConfig* current_config = matchServerConfig(host,
						conn->getListeningHost(), conn->getListeningPort());

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
					if (response.isCgi())
					{
						int cgi_fd = response.getCgiReadFd();
						int cgi_write_fd = response.getCgiWriteFd();
						_poller.addFd(cgi_fd, POLLIN);
						_poller.addFd(cgi_write_fd, POLLOUT);
						CgiState cgi_state;
						cgi_state.client = conn;
						cgi_state.pid = response.getCgiPid();
						cgi_state.readFd = cgi_fd;
						cgi_state.writeFd = cgi_write_fd;
						cgi_state.requestBody = std::string(conn->getParser().getBody().begin(), conn->getParser().getBody().end());
						cgi_state.requestBodyOffset = 0;
						cgi_state.requestBodyClosed = false;
						_cgi_states[cgi_fd] = cgi_state;
						_cgi_write_to_read[cgi_write_fd] = cgi_fd;
						if (cgi_state.requestBody.empty())
						{
							close(cgi_write_fd);
							_poller.removeFd(cgi_write_fd);
							_cgi_write_to_read.erase(cgi_write_fd);
							_cgi_states[cgi_fd].requestBodyClosed = true;
						}
						Logger::info("Waiting for CGI output on FD: " + StringUtils::to_string(cgi_fd));
					}
					else
					{
					queueResponse(conn, response);
					}
				}
					else if (parseState == RequestParser::PARSE_ERROR
						|| (state == RequestParser::STATE_PAYLOAD_TOO_LARGE && conn->getParser().isOversizedBodyDrained()))
				{
					Logger::warning("Request parsing error on FD: " + StringUtils::to_string(current_fd));

					HttpResponse response;
						ErrorPage::tryBuildDefault(state == RequestParser::STATE_PAYLOAD_TOO_LARGE ? 413 : 400, response);

					queueResponse(conn, response);
						_poller.setEvents(current_fd, POLLOUT);
				}
			}
		}
	}

	if (revents & POLLOUT)
	{
		if (_cgi_write_to_read.find(current_fd) != _cgi_write_to_read.end())
		{
			int read_fd = _cgi_write_to_read[current_fd];
			std::map<int, CgiState>::iterator cgiIt = _cgi_states.find(read_fd);
			if (cgiIt != _cgi_states.end())
			{
				CgiState& state = cgiIt->second;
				if (!state.requestBodyClosed && state.requestBodyOffset < state.requestBody.size())
				{
					ssize_t written = write(current_fd, state.requestBody.data() + state.requestBodyOffset,
						state.requestBody.size() - state.requestBodyOffset);
					if (written > 0)
						state.requestBodyOffset += static_cast<size_t>(written);
					else if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
					{
						Logger::warning("CGI stdin write failed on FD: " + StringUtils::to_string(current_fd));
						close(current_fd);
						_poller.removeFd(current_fd);
						close(state.readFd);
						_poller.removeFd(state.readFd);
						_cgi_write_to_read.erase(current_fd);
						_cgi_states.erase(cgiIt);
						continue;
					}
				}
				if (state.requestBodyOffset >= state.requestBody.size() && !state.requestBodyClosed)
				{
					close(current_fd);
					_poller.removeFd(current_fd);
					_cgi_write_to_read.erase(current_fd);
					state.requestBodyClosed = true;
				}
			}
		}
		else if(_connections.find(current_fd) != _connections.end())
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
						&& conn->getParser().getState() != RequestParser::STATE_PAYLOAD_TOO_LARGE
						&& !conn->shouldCloseAfterResponse();

					std::map<std::string, std::string>::const_iterator it = headers.find("connection");
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
