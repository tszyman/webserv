#include "core/Server.hpp"
#include "network/EventLoop.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"
#include "core/Config.hpp"
#include "network/SocketEngine.hpp"
#include <iostream>
#include <set>

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
    std::string config_path = "config/default.conf";

    if (argc == 2)
        config_path = argv[1];
    else if (argc > 2)
    {
        Logger::error("Too many arguments. Usage ./webserv [config_file]");
        return false;
    }

    Logger::info("Loading config from: " + config_path);

    if (!_config.loadConfig(config_path))
    {
        Logger::error("Failed to load configuration file.");
        return false;
    }

    Logger::info("Config loaded successfully.");
    return true;
}

void Server::run()
{
    Logger::info("Starting up network engines...");
    const std::vector<ServerConfig>& servers = _config.getServers();

    std::set<std::string> unique_endpoints;
    for (size_t i = 0; i < servers.size(); ++i)
    {
		unique_endpoints.insert(servers[i].host + ":" + StringUtils::to_string(servers[i].port));
    }

    std::vector<SocketEngine*> engines;
    
    for (size_t i = 0; i < servers.size(); ++i)
    {
		const std::string endpoint = servers[i].host + ":" + StringUtils::to_string(servers[i].port);
		if (unique_endpoints.find(endpoint) == unique_endpoints.end())
			continue;
		unique_endpoints.erase(endpoint);
        SocketEngine* engine = new SocketEngine(servers[i].host, servers[i].port);
    try
    {
        engine->init();
        engines.push_back(engine);
		Logger::info("Successfully initialized SocketEngine on " + endpoint);
    }
    catch (const std::exception& e)
    {
		Logger::error(std::string("Failed to initialize SocketEngine on ") + endpoint + ": " + e.what());
        delete engine;
    }
    }
    if (engines.empty())
    {
        Logger::error("No servers could be initialized. Shutting down.");
        return;
    }

    EventLoop loop(engines, servers);
    loop.run();

    for (size_t i = 0; i < engines.size(); ++i)
        delete engines[i];

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
