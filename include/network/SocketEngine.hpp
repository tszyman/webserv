#ifndef SOCKET_ENGINE_HPP
#define SOCKET_ENGINE_HPP

#include <string>
#include <netinet/in.h>
#include "Connection.hpp"
#include <sys/socket.h>
#include <fcntl.h>
#include <cstring>
#include <stdexcept>
#include <iostream>

class SocketEngine
{
    private:
        int         _server_fd;
        int         _port;
        struct sockaddr_in _address;

        SocketEngine(const SocketEngine& other);
        SocketEngine& operator=(const SocketEngine& other);

    public:
        SocketEngine(int port);
        ~SocketEngine();

        void init();
        Connection* acceptConnection(size_t maxBodySize);
        int getFd() const;        
        int getPort() const;
};

#endif