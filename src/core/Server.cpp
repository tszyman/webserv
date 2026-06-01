#include "core/Server.hpp"
#include "network/EventLoop.hpp"
#include <iostream>

Server::Server()
{   
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
    std::cout << "Server started" << std::endl;
    EventLoop loop;
    loop.run();
}