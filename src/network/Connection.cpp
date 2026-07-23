#include "network/Connection.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"
#include <iostream>

Connection::Connection(int fd, size_t maxBodySize, const std::string& listeningHost, int listeningPort)
	: _fd(fd), _parser(maxBodySize), _listening_host(listeningHost),
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
