#include "network/EventLoop.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"
#include "http/HttpErrorPage.hpp"
#include "http/HttpResponse.hpp"
#include "http/StatusCodes.hpp"
#include <fcntl.h>
#include <sys/wait.h>
#include <sstream>

// Set to 1 temporarily when inspecting individual requests.  Leave this off
// for stress tests: they deliberately generate tens of thousands of replies.
#define WEBSERV_LOG_RESPONSES 0

static std::string trimTrailingCrlf(const std::string& line)
{
	std::string result = line;
	if (!result.empty() && result[result.size() - 1] == '\r')
		result.erase(result.size() - 1);
	return result;
}

static bool findCgiHeaderEnd(const std::string& data, size_t& headerEnd)
{
	std::string::size_type separator = data.find("\r\n\r\n");
	if (separator != std::string::npos)
	{
		headerEnd = separator + 4;
		return true;
	}
	separator = data.find("\n\n");
	if (separator == std::string::npos)
		return false;
	headerEnd = separator + 2;
	return true;
}

static std::string buildChunkedCgiHead(const std::string& headerBlock)
{
	int statusCode = 200;
	bool hasContentType = false;
	std::string headers;
	std::string::size_type start = 0;
	while (start < headerBlock.size())
	{
		std::string::size_type end = headerBlock.find('\n', start);
		std::string line = headerBlock.substr(start, end == std::string::npos ? std::string::npos : end - start);
		line = trimTrailingCrlf(line);
		std::string::size_type colon = line.find(':');
		if (colon != std::string::npos)
		{
			std::string key = line.substr(0, colon);
			std::string value = line.substr(colon + 1);
			while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
				value.erase(0, 1);
			if (key == "Status")
				statusCode = std::atoi(value.c_str());
			else if (key == "Content-Length" || key == "Transfer-Encoding" || key == "Connection")
			{
			}
			else
			{
				if (key == "Content-Type")
					hasContentType = true;
				headers += key + ": " + value + "\r\n";
			}
		}
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	if (!hasContentType)
		headers += "Content-Type: text/plain\r\n";
	std::ostringstream response;
	response << "HTTP/1.1 " << statusCode << " " << HttpStatus::reasonPhrase(statusCode) << "\r\n";
	response << headers << "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n";
	return response.str();
}

static std::string makeChunk(const char* data, size_t length)
{
	std::ostringstream stream;
	stream << std::hex << length << "\r\n";
	std::string chunk = stream.str();
	chunk.append(data, length);
	chunk += "\r\n";
	return chunk;
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
	const size_t logPreviewLimit = 4096;
	std::string responsePreview = rawResponse.substr(0, logPreviewLimit);
	if (rawResponse.size() > logPreviewLimit)
	{
		responsePreview += "\n----- response body preview truncated; total response size: "
			+ StringUtils::to_string(rawResponse.size()) + " bytes -----";
	}
	Logger::debug("----- HTTP response for FD "
		+ StringUtils::to_string(connection->getFd())
		+ " -----\n" + responsePreview + "\n----- end HTTP response -----");
#endif

	connection->appendResponse(rawResponse);
	// A connection owns exactly one request at a time.  Once its response is
	// queued, leave subsequent bytes in the kernel buffer until this response
	// has been completely sent and the parser is reset.  Otherwise a persistent
	// client can feed its next request into a parser that is still in SUCCESS,
	// which turns it into a spurious 400 or appends responses out of order.
	if (connection->isDrainingAfterError())
		_poller.setEvents(connection->getFd(), POLLIN | POLLOUT);
	else
		_poller.setEvents(connection->getFd(), POLLOUT);
}

bool EventLoop::hasActiveCgi(Connection* connection) const
{
	for (std::map<int, CgiState>::const_iterator it = _cgi_states.begin();
		it != _cgi_states.end(); ++it)
	{
		if (it->second.client == connection)
			return true;
	}
	return false;
}

void EventLoop::cancelCgi(Connection* connection)
{
	for (std::map<int, CgiState>::iterator it = _cgi_states.begin();
		it != _cgi_states.end(); )
	{
		if (it->second.client != connection)
		{
			++it;
			continue;
		}
		const int readFd = it->second.readFd;
		const int writeFd = it->second.writeFd;
		kill(it->second.pid, SIGTERM);
		if (readFd != -1)
		{
			close(readFd);
			_poller.removeFd(readFd);
		}
		if (!it->second.requestBodyClosed)
			close(writeFd);
		_poller.removeFd(writeFd);
		_cgi_write_to_read.erase(writeFd);
		std::map<int, CgiState>::iterator toErase = it++;
		_cgi_states.erase(toErase);
	}
}

void EventLoop::reapFinishedCgis()
{
	for (std::map<int, CgiState>::iterator it = _cgi_states.begin();
		it != _cgi_states.end(); )
	{
		CgiState& state = it->second;
		if (!state.outputClosed)
		{
			++it;
			continue;
		}

		int status = 0;
		const pid_t result = waitpid(state.pid, &status, WNOHANG);
		if (result == 0)
		{
			++it;
			continue;
		}

		Connection* client_conn = state.client;
		const int write_fd = state.writeFd;
		const bool request_body_closed = state.requestBodyClosed;
		const bool response_headers_sent = state.responseHeadersSent;
		const std::string output = state.output;
		std::map<int, CgiState>::iterator to_erase = it++;
		_cgi_states.erase(to_erase);

		if (!request_body_closed)
		{
			close(write_fd);
			_poller.removeFd(write_fd);
		}
		_cgi_write_to_read.erase(write_fd);

		if (_connections.find(client_conn->getFd()) == _connections.end())
			continue;

		Logger::info("CGI process reaped for client FD: "
			+ StringUtils::to_string(client_conn->getFd()));
		if (response_headers_sent)
		{
			client_conn->appendResponse("0\r\n\r\n");
			_poller.setEvents(client_conn->getFd(), POLLOUT);
		}
		else
		{
			HttpResponse response;
			parseCgiOutput(output, response);
			queueResponse(client_conn, response);
		}
	}
}

void EventLoop::run()
{
	Logger::info("Starting the minimal event loop...");
	const int TIMEOUT_LIMIT = 15;
	const int CGI_TIMEOUT_LIMIT = 300;

	while (EventLoop::is_running)
	{
		reapFinishedCgis();

		std::map<int, Connection*>::iterator it = _connections.begin();
		while (it != _connections.end())
		{
			// Large uploads and CGI output can legitimately take longer than an
			// ordinary idle HTTP connection.  Keep the deadline finite so a CGI
			// cannot make a request wait indefinitely.
			const int timeout_limit = hasActiveCgi(it->second)
				? CGI_TIMEOUT_LIMIT : TIMEOUT_LIMIT;
			if (it->second->isTimedOut(timeout_limit))
			{
				int timeout_fd = it->first;
				Logger::warning("Connection timed out. Closing FD " + StringUtils::to_string(timeout_fd));
				cancelCgi(it->second);
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
				cancelCgi(_connections[current_fd]);
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
					// A larger chunk greatly reduces poll/send cycles for streamed CGI
					// output while still performing exactly one readiness-driven read.
					char cgi_buffer[65536];
					bool eof = false;
					// The pipe was reported readable by poll().  Read only once here:
					// poll() will report it again while more data is available.  This
					// deliberately avoids inspecting post-read error state, which the
					// project subject explicitly forbids.
					ssize_t bytes_read = read(current_fd, cgi_buffer, sizeof(cgi_buffer));
					if (bytes_read > 0)
					{
						if (state.responseHeadersSent)
						{
							state.client->appendResponse(makeChunk(cgi_buffer,
								static_cast<size_t>(bytes_read)));
							_poller.setEvents(state.client->getFd(), POLLOUT);
						}
						else
							state.output.append(cgi_buffer, bytes_read);
					}
					else if (bytes_read == 0)
					{
						eof = true;
					}
					else
					{
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
						}
						close(current_fd);
						_poller.removeFd(current_fd);
						_cgi_states.erase(cgiIt);
						continue;
					}

					if (!state.responseHeadersSent)
					{
						size_t headerEnd = 0;
						if (findCgiHeaderEnd(state.output, headerEnd))
						{
							state.client->appendResponse(buildChunkedCgiHead(state.output.substr(0, headerEnd)));
							state.responseHeadersSent = true;
							if (state.output.size() > headerEnd)
								state.client->appendResponse(makeChunk(state.output.data() + headerEnd,
									state.output.size() - headerEnd));
							state.output.clear();
							_poller.setEvents(state.client->getFd(), POLLOUT);
						}
					}

					if (eof)
					{
						// EOF only means the CGI closed stdout.  It may still be
						// running, so never wait for it here.  reapFinishedCgis()
						// will use waitpid(..., WNOHANG) on a later loop iteration.
						if (!state.requestBodyClosed)
						{
							close(state.writeFd);
							_poller.removeFd(state.writeFd);
							_cgi_write_to_read.erase(state.writeFd);
							state.requestBodyClosed = true;
						}
						close(current_fd);
						_poller.removeFd(current_fd);
						state.readFd = -1;
						state.outputClosed = true;
					}
				}
			}
		// B: Event on a client socket (Incoming data).
		else
		{
			// Request parsing is length-based, so a terminating NUL is not needed.
			// A 1 KiB receive buffer made the tester's 100 MiB CGI upload require
			// about 100,000 poll/recv iterations and hit its timeout before CGI
			// could respond.
			char buffer[65536];
			ssize_t bytes_read = recv(current_fd, buffer, sizeof(buffer), 0);
			Connection* conn = _connections[current_fd];

			// Graceful disconnection
			if (bytes_read == 0)
			{
				Logger::info(std::string("Client gracefully disconnected on FD: ") + StringUtils::to_string(current_fd));
				Connection* closingConnection = _connections[current_fd];
				cancelCgi(closingConnection);
				delete closingConnection;
				_connections.erase(current_fd);
				_poller.removeFd(current_fd);
			}
			//Read error (return -1)
			else if (bytes_read < 0)
			{
				Logger::warning(std::string("recv() error on FD: ") + StringUtils::to_string(current_fd));
				Connection* closingConnection = _connections[current_fd];
				cancelCgi(closingConnection);
				delete closingConnection;
				_connections.erase(current_fd);
				_poller.removeFd(current_fd);
			}
			// Successful data read
			else
			{
				conn->updateLastActivity();
				// A malformed request can still have bytes in flight.  Do not feed
				// them back into the parser after responding; discard them until the
				// client observes Connection: close and closes its side.
				if (conn->isDrainingAfterError())
					continue;
				RequestParser& parser = conn->getParser();
				const size_t headerBytes = parser.feedHeaders(buffer,
					static_cast<size_t>(bytes_read));

				// For Content-Length CGI POSTs, create CGI immediately after the
				// headers.  Body bytes are kept only until the non-blocking CGI pipe
				// accepts them, instead of first storing the entire request in the
				// RequestParser.
				if (!hasActiveCgi(conn) && parser.headersComplete()
					&& parser.hasContentLengthBody() && !parser.isBodyComplete())
				{
					const std::map<std::string, std::string>& earlyHeaders = parser.getHeaders();
					std::string earlyHost;
					std::map<std::string, std::string>::const_iterator hostIt = earlyHeaders.find("host");
					if (hostIt != earlyHeaders.end())
						earlyHost = hostIt->second;
					const ServerConfig* earlyConfig = matchServerConfig(earlyHost,
						conn->getListeningHost(), conn->getListeningPort());
					if (earlyConfig != NULL)
					{
						Router earlyRouter;
						for (size_t locationIndex = 0; locationIndex < earlyConfig->locations.size(); ++locationIndex)
							earlyRouter.addLocation(earlyConfig->locations[locationIndex]);
						if (earlyRouter.isCgiRequest(parser))
						{
							HttpResponse cgiResponse;
							earlyRouter.route(parser, cgiResponse);
							if (cgiResponse.isCgi())
							{
								const int cgiFd = cgiResponse.getCgiReadFd();
								const int cgiWriteFd = cgiResponse.getCgiWriteFd();
								_poller.addFd(cgiFd, POLLIN);
								_poller.addFd(cgiWriteFd, POLLOUT);
								CgiState cgiState;
								cgiState.client = conn;
								cgiState.pid = cgiResponse.getCgiPid();
								cgiState.readFd = cgiFd;
								cgiState.writeFd = cgiWriteFd;
								cgiState.requestBody.clear();
								cgiState.requestBodyOffset = 0;
								cgiState.requestBodyClosed = false;
								cgiState.requestBodyComplete = false;
								cgiState.responseHeadersSent = false;
								cgiState.outputClosed = false;
								_cgi_states[cgiFd] = cgiState;
								_cgi_write_to_read[cgiWriteFd] = cgiFd;
								conn->closeAfterResponse();
								_poller.setEvents(conn->getFd(), POLLIN);
							}
						}
					}
				}

				if (hasActiveCgi(conn))
				{
					for (std::map<int, CgiState>::iterator active = _cgi_states.begin();
						active != _cgi_states.end(); ++active)
					{
						if (active->second.client != conn)
							continue;
						const size_t bodyBytes = static_cast<size_t>(bytes_read) - headerBytes;
						if (bodyBytes > 0)
						{
							if (active->second.requestBodyOffset == active->second.requestBody.size())
							{
								active->second.requestBody.clear();
								active->second.requestBodyOffset = 0;
							}
							else if (active->second.requestBodyOffset >= 65536
								&& active->second.requestBodyOffset
									>= active->second.requestBody.size() / 2)
							{
								active->second.requestBody.erase(0,
									active->second.requestBodyOffset);
								active->second.requestBodyOffset = 0;
							}
							active->second.requestBody.append(buffer + headerBytes, bodyBytes);
							parser.consumeStreamingBody(buffer + headerBytes, bodyBytes);
						}
						active->second.requestBodyComplete = parser.isBodyComplete();
						break;
					}
					continue;
				}

				parser.feedBody(buffer + headerBytes,
					static_cast<size_t>(bytes_read) - headerBytes);
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
						cgi_state.requestBodyComplete = true;
						cgi_state.responseHeadersSent = false;
						cgi_state.outputClosed = false;
						_cgi_states[cgi_fd] = cgi_state;
						_cgi_write_to_read[cgi_write_fd] = cgi_fd;
						conn->closeAfterResponse();
						// CGI execution is asynchronous, but its client request is
						// still in progress.  Do not parse a second request on this
						// socket until the CGI response has been sent.
						_poller.setEvents(conn->getFd(), 0);
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
					const bool oversized = state == RequestParser::STATE_PAYLOAD_TOO_LARGE;
					ErrorPage::tryBuildDefault(oversized ? 413 : 400, response);
					// For a generic parse error the parser cannot know whether all
					// client bytes have arrived.  Keeping the connection open long
					// enough to discard them avoids a TCP RST while a client is writing.
					if (!oversized)
						conn->startErrorDrain();

					queueResponse(conn, response);
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
				if (!state.requestBodyClosed
					&& state.requestBodyOffset < state.requestBody.size())
				{
					ssize_t written = write(current_fd,
						state.requestBody.data() + state.requestBodyOffset,
						state.requestBody.size() - state.requestBodyOffset);
					if (written > 0)
					{
						state.requestBodyOffset += static_cast<size_t>(written);
						if (state.requestBodyOffset == state.requestBody.size())
						{
							state.requestBody.clear();
							state.requestBodyOffset = 0;
						}
					}
					else if (written < 0)
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
				if (state.requestBodyComplete
					&& state.requestBodyOffset == state.requestBody.size()
					&& !state.requestBodyClosed)
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

			if (conn->hasPendingResponse())
			{
				ssize_t bytes_sent = send(current_fd, conn->getResponseData(),
					conn->getResponseSize(), 0);

				if (bytes_sent > 0)
				{
					Logger::debug(std::string("Sent ") + StringUtils::to_string(bytes_sent) + " bytes to FD " + StringUtils::to_string(current_fd));
					conn->eraseSentData(bytes_sent);
					conn->updateLastActivity();
				}
				else if (bytes_sent < 0)
				{
					Logger::error(std::string("Send() error on FD: ") + StringUtils::to_string(current_fd));
				}

				if (!conn->hasPendingResponse())
				{
					// A chunk of a streaming CGI response has been sent, but the
					// CGI process can still produce more chunks.  Keep the parser
					// untouched until its terminating chunk has been queued and sent.
					if (hasActiveCgi(conn))
					{
						_poller.setEvents(current_fd, 0);
						continue;
					}
					if (conn->isDrainingAfterError())
					{
						_poller.setEvents(current_fd, POLLIN);
						continue;
					}
					const std::map<std::string, std::string>& headers = conn->getParser().getHeaders();
					// A malformed or oversized request cannot safely be reused as a
					// persistent connection: parsing may have stopped before all
					// headers (including Connection) were read.
					bool keep_alive = !conn->mustCloseAfterResponse()
						&& conn->getParser().getState() != RequestParser::STATE_ERROR
						&& conn->getParser().getState() != RequestParser::STATE_PAYLOAD_TOO_LARGE;

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
