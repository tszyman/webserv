#include "cgi/CgiEnv.hpp"
#include <cstdlib>
#include <cstring>

static char toUpperAscii(char c)
{
	if (c >= 'a' && c <= 'z')
		return static_cast<char>(c - 'a' + 'A');
	return c;
}

CgiEnv::CgiEnv(const RequestParser& request, const std::string& scriptPath) : _envp(NULL) {
	// 1. Standard CGI variables
	_envMap["GATEWAY_INTERFACE"] = "CGI/1.1";
	_envMap["SERVER_PROTOCOL"] = request.getVersion() ;
	_envMap["SERVER_SOFTWARE"] = "webserv/1.0";
	_envMap["REQUEST_METHOD"] = request.getMethod();

	_envMap["PATH_INFO"] = request.getPath();
	_envMap["QUERY_STRING"] = request.getQuery();

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
			key[i] = toUpperAscii(key[i]);
		}
		_envMap[key] = it->second;
	}

	// CGI exposes these two entity headers without the HTTP_ prefix.
	std::map<std::string, std::string>::const_iterator contentLength = headers.find("content-length");
	if (contentLength != headers.end())
		_envMap["CONTENT_LENGTH"] = contentLength->second;
	std::map<std::string, std::string>::const_iterator contentType = headers.find("content-type");
	if (contentType != headers.end())
		_envMap["CONTENT_TYPE"] = contentType->second;

	_buildEnvpArray();
}

CgiEnv::~CgiEnv() {
	if (_envp)
	{
		for (size_t i = 0; _envp[i] != NULL; ++i)
		{
			delete[] _envp[i];
		}
		delete[] _envp;
	}
}

void CgiEnv::_buildEnvpArray() {
	_envp = new char*[_envMap.size() + 1];
	size_t i = 0;
	for (std::map<std::string, std::string>::iterator it = _envMap.begin(); it != _envMap.end(); ++it)
	{
		std::string envString = it->first + "=" + it->second;
		_envp[i] = new char[envString.size() + 1];
		for (size_t j = 0; j < envString.size(); ++j)
			_envp[i][j] = envString[j];
		_envp[i][envString.size()] = '\0';
		i++;
	}
	_envp[i] = NULL;
}

char** CgiEnv::getEnv() const {
	return _envp;
}
