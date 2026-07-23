#ifndef SOCKET_ENGINE_HPP
#define SOCKET_ENGINE_HPP

#include <string>
#include "Connection.hpp"
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <cstring>
#include <stdexcept>
#include <iostream>

class SocketEngine
{
    private:
        int         _server_fd;
		std::string _host;
        int         _port;

        SocketEngine(const SocketEngine& other);
        SocketEngine& operator=(const SocketEngine& other);

    public:
        SocketEngine(int port);
		SocketEngine(const std::string& host, int port);
        ~SocketEngine();

        void init();
        Connection* acceptConnection(size_t maxBodySize);
        int getFd() const;        
        int getPort() const;
		const std::string& getHost() const;
};

#endif
