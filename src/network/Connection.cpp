#include "network/Connection.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"
#include <iostream>

Connection::Connection(int fd, size_t maxBodySize) : _fd(fd), _response_buffer(), _request_buffer(), _parser(maxBodySize), _max_body_size(maxBodySize)
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

void Connection::appendResponse(const std::string& data)
{
    _response_buffer += data;
}

std::string& Connection::getResponseBuffer()
{
    return _response_buffer;
}

void Connection::eraseSentData(size_t bytes)
{
    _response_buffer.erase(0, bytes);
}

void Connection::appendRequestData(const char* data, size_t bytes)
{
	_request_buffer.append(data, bytes);
}

bool Connection::hasRequestData() const
{
	return !_request_buffer.empty();
}

void Connection::parseRequestData()
{
	if (_request_buffer.empty())
		return;
	const size_t consumed = _parser.feed(_request_buffer.data(), _request_buffer.size());
	_request_buffer.erase(0, consumed);
}

void Connection::reset()
{
	_parser = RequestParser(_max_body_size);
	_response_buffer.clear();
    updateLastActivity();
}

void Connection::updateLastActivity()
{
    _last_activity = time(NULL);
}

bool Connection::isTimedOut(int timeout_seconds) const
{
    return difftime(time(NULL), _last_activity) > timeout_seconds;
}
