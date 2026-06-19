#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include "routing/LocationConfig.hpp"

// Represents a single server {} block from the config file
struct ServerConfig {
	int port;
	std::string serverName;
	std::vector<LocationConfig> locations;
};

class Config {

};

#endif
