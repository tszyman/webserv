#include "core/Server.hpp"
#include "network/EventLoop.hpp"
#include "utils/Logger.hpp"
#include <iostream>

Server::Server()
{   
}

Server::~Server()
{
    std::map<int, Connection*>::iterator it;
    for (it = _client_connections.begin(); it != _client_connections.end(); ++it)
    {
        delete it->second;
    }
    _client_connections.clear();
}

bool Server::loadConfig(int argc, char **argv)
{
    std::cout << "Loading config..." << std::endl;
    (void)argc;
    (void)argv;
    std::cout << "Config loaded." << std::endl;
    return true;
}

void Server::run()
{
    Logger::info("Starting up...");
    SocketEngine engine(8080);

    try
    {
        engine.init();
    }
    catch (const std::exception& e)
    {
        Logger::error(std::string("Failed to initialize SocketEngine: ") + e.what());
        return;
    }

    EventLoop loop(&engine);

    loop.run();

}

void Server::addConnection(Connection* conn)
{
    if (conn != NULL)
    {
        int fd = conn->getFd();
        _client_connections[fd] = conn;
        std::cout << "[Server] Mapping created for FD: " << fd << std::endl;
    }
}

void Server::cleanupConnection(int fd)
{
    std::map<int, Connection*>::iterator it = _client_connections.find(fd);

    if (it != _client_connections.end())
    {
        std::cout << "[Server] Cleaning up and removing connection for FD: " << fd << std::endl;
        delete it->second;
        _client_connections.erase(it);
    }
}