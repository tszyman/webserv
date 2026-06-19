#include "core/Config.hpp"
#include "utils/Logger.hpp"

#include <fstream>
#include <sstream>
#include <cstdlib>

Config::Config() : _currentTokenIndex(0) {}
Config::~Config() {}

void Config::tokenize(const std::string& fileContent)
{

}

bool Config::loadConfig(const std::string& fileName)
{

}

void Config::parseServerBlock()
{

}

void Config::parseLocationBlock(ServerConfig& server)
{

}

const std::vector<ServerConfig>& Config::getServers() const
{
	return _servers;
}
