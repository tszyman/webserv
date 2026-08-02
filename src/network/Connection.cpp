#include "network/Connection.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"
#include <iostream>

Connection::Connection(int fd, size_t maxBodySize, const std::string& listeningHost, int listeningPort)
	: _fd(fd), _response_offset(0), _parser(maxBodySize), _drain_after_error(false), _close_after_response(false), _listening_host(listeningHost),
	_listening_port(listeningPort), _max_body_size(maxBodySize)
{
    Logger::info(std::string("New connection created on FD: ") + StringUtils::to_string(_fd));
    _last_activity = time(NULL);
}

Connection::~Connection()
{
    if (_fd != -1)
    {
        Logger::info(std::string("Closing connection on FD: ") + StringUtils::to_string(_fd));
        close(_fd); //Close client socket when object is destroyed
    }
}

int Connection::getFd() const
{
    return _fd;
}

const std::string& Connection::getListeningHost() const
{
	return _listening_host;
}

int Connection::getListeningPort() const
{
	return _listening_port;
}

void Connection::appendResponse(const std::string& data)
{
	// Do not retain an already-sent response.  When only part of a large CGI
	// response was sent, compact occasionally instead of after every send().
	// This keeps response consumption amortized O(n), rather than O(n^2).
	if (_response_offset == _response_buffer.size())
	{
		_response_buffer.clear();
		_response_offset = 0;
	}
	else if (_response_offset >= 65536
		&& _response_offset >= _response_buffer.size() / 2)
	{
		_response_buffer.erase(0, _response_offset);
		_response_offset = 0;
	}
    _response_buffer += data;
}

const char* Connection::getResponseData() const
{
	return _response_buffer.data() + _response_offset;
}

size_t Connection::getResponseSize() const
{
	return _response_buffer.size() - _response_offset;
}

bool Connection::hasPendingResponse() const
{
	return _response_offset < _response_buffer.size();
}

void Connection::eraseSentData(size_t bytes)
{
	const size_t remaining = getResponseSize();
	if (bytes >= remaining)
	{
		_response_buffer.clear();
		_response_offset = 0;
	}
	else
		_response_offset += bytes;
}

void Connection::reset()
{
    _parser = RequestParser(_max_body_size);
    _response_buffer.clear();
	_response_offset = 0;
	_drain_after_error = false;
	_close_after_response = false;
    updateLastActivity();
}

void Connection::startErrorDrain()
{
	_drain_after_error = true;
}

bool Connection::isDrainingAfterError() const
{
	return _drain_after_error;
}

void Connection::closeAfterResponse()
{
	_close_after_response = true;
}

bool Connection::mustCloseAfterResponse() const
{
	return _close_after_response;
}

void Connection::updateLastActivity()
{
    _last_activity = time(NULL);
}

bool Connection::isTimedOut(int timeout_seconds) const
{
    return difftime(time(NULL), _last_activity) > timeout_seconds;
}
