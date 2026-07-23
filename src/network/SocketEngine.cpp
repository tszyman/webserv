#include "network/SocketEngine.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"

SocketEngine::SocketEngine(int port) : _server_fd(-1), _host("0.0.0.0"), _port(port)
{
}

SocketEngine::SocketEngine(const std::string& host, int port)
    : _server_fd(-1), _host(host), _port(port)
{
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
    struct addrinfo hints = {};
    struct addrinfo* addresses = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    const char* bindHost = _host.c_str();
    const std::string portText = StringUtils::to_string(_port);
    const int addressResult = getaddrinfo(bindHost, portText.c_str(), &hints, &addresses);
    if (addressResult != 0)
        throw std::runtime_error(std::string("Error: getaddrinfo() failed: ") + gai_strerror(addressResult));

    for (struct addrinfo* current = addresses; current != NULL; current = current->ai_next)
    {
        _server_fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (_server_fd == -1)
            continue;

        int opt = 1;
        if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1
            || fcntl(_server_fd, F_SETFL, O_NONBLOCK) == -1
            || fcntl(_server_fd, F_SETFD, FD_CLOEXEC) == -1)
        {
            close(_server_fd);
            _server_fd = -1;
            continue;
        }
        if (bind(_server_fd, current->ai_addr, current->ai_addrlen) == 0)
            break;
        close(_server_fd);
        _server_fd = -1;
    }
    freeaddrinfo(addresses);
    if (_server_fd == -1)
        throw std::runtime_error("Error: bind() failed for " + _host + ":" + portText);
    if (listen(_server_fd, SOMAXCONN) == -1)
    {
        close(_server_fd);
        _server_fd = -1;
        throw std::runtime_error("Error: listen() failed");
    }

    Logger::info("Server started on " + _host + ":" + portText + " (FD: " + StringUtils::to_string(_server_fd) + ")");
}

Connection* SocketEngine::acceptConnection(size_t maxBodySize)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    //Accept the incoming connection
    int client_fd = accept(_server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd == -1)
    {
        return NULL;
    }
    
    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1 || fcntl(client_fd, F_SETFD, FD_CLOEXEC) == -1)
    {
        Logger::error(std::string("fcntl failed for FD ") + StringUtils::to_string(client_fd));
        close(client_fd);
        return NULL;
    }
    
    try
    {
		return new Connection(client_fd, maxBodySize, _host, _port);
    }
    catch (const std::exception& e)
    {
        Logger::error(std::string("Critical allocation error for FD ") + StringUtils::to_string(client_fd) + ": " + e.what());
        close(client_fd);
        return NULL;
    }
}

int SocketEngine::getFd() const
{
    return _server_fd;
}

int SocketEngine::getPort() const
{
    return _port;
}

const std::string& SocketEngine::getHost() const
{
	return _host;
}
