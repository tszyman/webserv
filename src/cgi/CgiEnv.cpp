#include "cgi/CgiEnv.hpp"
#include <cstdlib>
#include <cstring>

CgiEnv::CgiEnv(const RequestParser& request, const std::string& scriptPath) : _envp(NULL) {
	// 1. Standard CGI variables
	_envMap["GATEWAY_INTERFACE"] = "CGI/1.1";
	_envMap["SERVER_PROTOCOL"] = request.getVersion() ;
	_envMap["SERVER_SOFTWARE"] = "webserv/1.0";
	_envMap["REQUEST_METHOD"] = request.getMethod();

	// Split path and query string
	std::string fullPath = request.getPath();
	size_t questionMarkPos = fullPath.find('?');
	if (questionMarkPos != std::string::npos)
	{
		_envMap["PATH_INFO"] = fullPath.substr(0, questionMarkPos);
		_envMap["QUERY_STRING"] = fullPath.substr(questionMarkPos + 1);
	}
	else
	{
		_envMap["PATH_INFO"] = fullPath;
		_envMap["QUERY_STRING"] = "";
	}

	_envMap["SCRIPT_FILENAME"] = scriptPath;

	// 2. HTTP headers mapping
	const std::map<std::string, std::string>& headers = request.getHeaders();
	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
	{
		std::string key = "HTTP_" + it->first;
		for (size_t i = 0; i < key.length(); ++i)
		{
			if (key[i] == '-')
				key[i] = '_';
			key[i] = ::toupper(key[i]);
		}
		_envMap[key] = it->second;
	}

	// Specific handling for Content-Length and Content-Path
	if (headers.count("Content-Length"))
	{
		_envMap["CONTENT_LENGTH"] = headers.at("Content-Length");
	}
	if (headers.count("Content-Type"))
	{
		_envMap["CONTENT_TYPE"] = headers.at("Content-Type");
	}

	_buildEnvpArray();
}

CgiEnv::~CgiEnv() {
	if (_envp)
	{
		for (size_t i = 0; _envp[i] != NULL; ++i)
		{
			std::free(_envp[i]);
		}
		delete[] _envp;
	}
}

void CgiEnv::_buildEnvpArray() {
	_envp = new char*[_envMap.size() + 1];
	size_t i = 0;
	for (std::map<std::string, std::string>::iterator it = _envMap.begin(); it != _envMap.end(); ++it)
	{
		std::string envString = it->first + "=" = it->second;
		_envp[i] = strdup(envString.c_str());
		i++;
	}
	_envp[i] = NULL;
}

char** CgiEnv::getEnv() const {
	return _envp;
}
