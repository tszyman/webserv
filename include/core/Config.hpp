#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include "routing/LocationConfig.hpp"

// Represents a single server {} block from the config file
struct ServerConfig {
	int port;
	std::string serverName;
	std::vector<LocationConfig> locations;
	size_t clientMaxBodySize;

	// clientMaxBodySize: Default to 1MB (1048576 bytes) if not specified, or 0 for unlimited
	ServerConfig() : port(8080), serverName("localhost"), clientMaxBodySize(1048576) {}
};

class Config {
	public:
		Config();
		~Config();

		bool loadConfig(const std::string& filename);
		const std::vector<ServerConfig>& getServers() const;

	private:
		std::vector<ServerConfig> _servers;
		std::vector<std::string> _tokens;
		size_t _currentTokenIndex;

		//Core parsing steps/methods
		void tokenize(const std::string& fileContent);
		void parseServerBlock();
		void parseLocationBlock(ServerConfig& server);

};

#endif
