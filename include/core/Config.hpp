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

	ServerConfig() : port(8080), serverName("localhost") {}
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
