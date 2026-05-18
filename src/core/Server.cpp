#include "core/Server.hpp"
#include "network/EventLoop.hpp"
#include <stdio.h>

Server::Server()
{   
}

bool Server::loadConfig(int argc, char **argv)
{
    printf("Loading config...\n");
    (void)argc;
    (void)argv;
    printf("Config loaded\n");
    return true;
}

void Server::run()
{
    printf("Server started\n");
    EventLoop loop;
    loop.run();
}