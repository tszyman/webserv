#include "network/Connection.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"
#include <iostream>

Connection::Connection(int fd) : _fd(fd)
{
    Logger::info(std::string("New connection created on FD: ") + StringUtils::to_string(_fd));
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