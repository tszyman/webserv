#include "network/Connection.hpp"
#include <iostream>

Connection::Connection(int fd) : _fd(fd)
{
    std::cout << "[Connection] New connection created on FD: " << _fd << std::endl;
}

Connection::~Connection()
{
    if (_fd != -1)
    {
        std::cout << "[Connection] Closing connection on FD: " << _fd << std::endl;
        close(_fd); //Close client socket when object is destroyed
    }
}

int Connection::getFd() const
{
    return _fd;
}