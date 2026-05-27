#include "SocketEngine.hpp"

SocketEngine::SocketEngine(int port) : _server_fd(-1), _port(port)
{
    std::memset(&_address, 0, sizeof(_address));
    _address.sin_family = AF_INET;
    _address.sin_addr.s_addr = INADDR_ANY;
    _address.sin_port = htons(_port);
}

SocketEngine::~SocketEngine()
{
    if (_server_fd != -1)
    {
        close(_server_fd);
    }
}

void SocketEngine::init()
{
    // 1. Create socket (IPv4, TCP)
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd == -1)
    {
        throw std::runtime_error("Error: socket() failed");
    }

    // 2. SO_REUSEADDR - allows immediate server restart without "Address already in use!" error
    int opt = 1;
    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        throw std::runtime_error("Error: sersockopt() failed");
    }

    // 3. Set socket to non-blocking mode (Non-blocking I/O)
    if (fcntl(_server_fd, F_SETFL, O_NONBLOCK) == -1)
    {
        throw std::runtime_error("Error: fcntl() failed");
    }

    // 4. BNind socket to address and port
    if (bind(_server_fd, (struct sockaddr*)&_address, sizeof(_address)) == -1)
    {
        throw std::runtime_error("Error: bind() failed");
    }

    // 5. Start listening (backlog = SOMAXCONN for maximum queue size)
    if (listen(_server_fd, SOMAXCONN) == -1)
    {
        throw std::runtime_error("Error: listen() failed");
    }

    std::cout << "[SocketEngine] Server started and listening on port " << _port << "(FD: " << _server_fd << ")" << std::endl; 
}