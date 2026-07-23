#include "core/Config.hpp"
#include "utils/Logger.hpp"

#include <fstream>
#include <sstream>
#include <cstdlib>

Config::Config() : _currentTokenIndex(0) {}
Config::~Config() {}

void Config::tokenize(const std::string& fileContent)
{
	std::string token = "";
	for (size_t i = 0; i < fileContent.length(); ++i)
	{
		char c = fileContent[i];

		// 1. NGINX Comment handling
		if (c == '#')
		{
			while ( i < fileContent.length() && fileContent[i] != '\n')
			{
				i++;
			}
			// Skip to the next line of the loop (ignoring the new line itself)
			continue;
		}
		
		// 2. Break on whitespace
		if (isspace(c))
		{
			if (!token.empty())
			{
				_tokens.push_back(token);
				token.clear();
			}
		}

		// 3. Separate structural characters so "{", "}" and ";" become their own tokens
		else if (c == '{' || c == '}' || c == ';')
		{
			if (!token.empty())
			{
				_tokens.push_back(token);
				token.clear();
			}
			_tokens.push_back(std::string(1,c));
		}
		else
		{
			token += c;
		}
	}
	if (!token.empty())
		_tokens.push_back(token);
}

bool Config::loadConfig(const std::string& filename)
{
	std::ifstream file(filename.c_str());
	if (!file.is_open())
	{
		Logger::error("Config: Could not open file: " + filename);
		return false;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	tokenize(buffer.str());

	try {
		while (_currentTokenIndex < _tokens.size())
		{
			if (_tokens[_currentTokenIndex] == "server")
			{
				_currentTokenIndex++;
				if (_currentTokenIndex >= _tokens.size() || _tokens[_currentTokenIndex] != "{")
				{
					throw std::runtime_error("Expected '{' after 'server'");
				}
				_currentTokenIndex++; // skip '{'
				parseServerBlock();
			}
			else
			{
				throw std::runtime_error("Unknown directive outside server block: " + _tokens[_currentTokenIndex]);
			}
		}
	} catch (const std::exception& e) {
		Logger::error(std::string("Configuration Parsing Error: ") + e.what());
		return false;
	}
	return true;
}

void Config::parseServerBlock()
{
	ServerConfig newServer;

	while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != "}")
	{
		std::string directive = _tokens[_currentTokenIndex++];

		if (directive == "listen")
		{
			if(_currentTokenIndex >= _tokens.size()) 
				throw std::runtime_error("Unexpected EOF after listen");
			newServer.port = std::atoi(_tokens[_currentTokenIndex++].c_str());
			if (_currentTokenIndex >= _tokens.size() || _tokens[_currentTokenIndex++] != ";")
				throw std::runtime_error("Expected ';' after listen directive");
		}
		else if (directive == "server_name")
		{
			if(_currentTokenIndex >= _tokens.size()) 
				throw std::runtime_error("Unexpected EOF after server_name");
			newServer.serverName = _tokens[_currentTokenIndex++];
			if(_currentTokenIndex >= _tokens.size() || _tokens[_currentTokenIndex++] != ";")
				throw std::runtime_error("Expected ';' after server_name directive");
		}
		else if (directive == "location")
		{
			parseLocationBlock(newServer);
		}
		else if (directive == "client_max_body_size")
		{
			if(_currentTokenIndex >= _tokens.size())
				throw std::runtime_error("Unexpected EOF after clinent_max_body_size");
			newServer.clientMaxBodySize = static_cast<size_t>(std::atoi(_tokens[_currentTokenIndex++].c_str()));
			if(_currentTokenIndex >= _tokens.size() || _tokens[_currentTokenIndex++] != ";")
				throw std::runtime_error("Expected ':' after client_max_body_size");
		}
		else
		{
			throw std::runtime_error("Unknown directive in server block: " + directive);
		}
	}

	if (_currentTokenIndex >= _tokens.size())
	{
		throw std::runtime_error("Unexpected end of file, missing '}' for server block ");
	}

	_currentTokenIndex++; // Skip '}'
	_servers.push_back(newServer);
}

void Config::parseLocationBlock(ServerConfig& server)
{
	if (_currentTokenIndex >= _tokens.size())
		throw std::runtime_error("Unexpected EOF reading location path");
	std::string path = _tokens[_currentTokenIndex++];

	if (_currentTokenIndex >= _tokens.size() || _tokens[_currentTokenIndex++] != "{")
		throw std::runtime_error("Expected '{' after location path " + path);

	// Gathering variables first to satisfy LocationConfig's strict constructor
	std::string root = "";
	std::vector<std::string> allowedMethods;
	std::vector<std::string> indexFiles;
	bool autoindex = false;
	bool uploadEnabled = false;
	std::string uploadStore = "";
	size_t clientMaxBodySize = 0;

	while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != "}")
	{
		std::string directive = _tokens[_currentTokenIndex++];

		if (directive == "root")
		{
			if (_currentTokenIndex >= _tokens.size())
				throw std::runtime_error("Unexpected EOF after root");
			root = _tokens[_currentTokenIndex++];
			if (_currentTokenIndex >= _tokens.size() || _tokens[_currentTokenIndex++] != ";")
				throw std::runtime_error("Expected ';' after root directive");
		}
		else if (directive == "allowed_methods")
		{
			while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != ";")
			{
				allowedMethods.push_back(_tokens[_currentTokenIndex++]);
			}
			if (_currentTokenIndex >= _tokens.size())
				throw std::runtime_error("Expected ';' after allowed_methods");
			_currentTokenIndex++; // Skip ';'
		}
		else if (directive == "autoindex")
		{
			if(_currentTokenIndex >= _tokens.size())
				throw std::runtime_error("Unexpected EOF after autoindex");
			std::string val = _tokens[_currentTokenIndex++];
			autoindex = (val == "on");
			if(_currentTokenIndex >= _tokens.size() || _tokens[_currentTokenIndex++] != ";")
				throw std::runtime_error("Expacted ';' after autoindex");
		}
		else if (directive == "upload_enable")
		{
			if(_currentTokenIndex >= _tokens.size())
				throw std::runtime_error("Unexpected EOF after uplopad_enable");
			std::string val = _tokens[_currentTokenIndex++];
			uploadEnabled = (val == "on");
			if(_currentTokenIndex >= _tokens.size() || _tokens[_currentTokenIndex++] != ";")
				throw std::runtime_error("Expacted ';' after upload_enable");
		}
		else if (directive == "upload_store")
		{
			if(_currentTokenIndex >= _tokens.size())
				throw std::runtime_error("Unexpected EOF after uplopad_store");
			uploadStore = _tokens[_currentTokenIndex++];
			if(_currentTokenIndex >= _tokens.size() || _tokens[_currentTokenIndex++] != ";")
				throw std::runtime_error("Expacted ';' after upload_store");
		}
		else if (directive == "index")
		{
			while (_currentTokenIndex < _tokens.size() && _tokens[_currentTokenIndex] != ";")
			{
				indexFiles.push_back(_tokens[_currentTokenIndex++]);
			}
			if (_currentTokenIndex >= _tokens.size())
				throw std::runtime_error("Expected ';' after index directive");
			_currentTokenIndex++; // Skip ';'
		}
		else if (directive == "client_max_body_size")
		{
			if (_currentTokenIndex >= _tokens.size())
				throw std::runtime_error("Unexpected EOF after client_max_body_size");
			clientMaxBodySize = static_cast<size_t>(std::atoi(_tokens[_currentTokenIndex++].c_str()));
			if (_currentTokenIndex >= _tokens.size() || _tokens[_currentTokenIndex++] != ";")
				throw std::runtime_error("Expected ';' after client_max_body_size directive");
		}
		else
		{
			throw std::runtime_error("Unknown directive in location block: " + directive);
		}
	}

	if (_currentTokenIndex >= _tokens.size())
		throw std::runtime_error("Unexpected EOF, missing '}' in location block");
	_currentTokenIndex++; // Skip '}'

	//Building the LocationConfig safely
	LocationConfig newLocation(path, root);
	for (size_t i = 0; i < allowedMethods.size(); ++i)
	{
		newLocation.addAllowedMethod(allowedMethods[i]);
	}
	newLocation.setIndexFiles(indexFiles);
	newLocation.setAutoindex(autoindex);
	if(uploadEnabled)
	{
		newLocation.setUpload(true,uploadStore);
	}
	newLocation.setClientMaxBodySize(clientMaxBodySize);
	server.locations.push_back(newLocation);
}

const std::vector<ServerConfig>& Config::getServers() const
{
	return _servers;
}
