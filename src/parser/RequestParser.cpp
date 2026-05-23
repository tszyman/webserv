#include "parser/RequestParser.hpp"
#include <string>

RequestParser::RequestParser()
{
}
std::string RequestParser::trim(const std::string &str)
{
	std::string::size_type	start;
	std::string::size_type	end;

	start = 0;
	while(start < str.size() && (str[start] == ' ' || str[start] == '\t'))
		start++;

	end = str.size();
	while(end > start && (str[end-1] == ' ' || str[end - 1] == '\t'))
		end--;

	return str.substr(start, end - start);
}

bool RequestParser::parseRequestLine(const std::string &line)
{
	std::string::size_type pos1;
	std::string::size_type pos2;

	pos1 = line.find(' ');
	if(pos1 == std::string::npos)
		return false;

	pos2 = line.find(' ', pos1 + 1);
	if(pos2 == std::string::npos)
		return false;

	_method = line.substr(0, pos1);
	_path = line.substr(pos1 + 1, pos2 - pos1 - 1);
	_version = line.substr(pos2 + 1);

	if(_method != "GET" && _method != "POST" && _method != "DELETE")
		return false;

	if(_version != "HTTP/1.1" && _version != "HTTP/1.0")
		return false;

	if(_path.empty() || _path[0] != '/')
		return false;

	return true;
}

bool RequestParser::parseHeaderLine(const std::string &line)
{
	std::string::size_type pos;
	std::string key;
	std::string value;
	std::string::size_type i;

	if(line.empty())
		return true;

	pos = line.find(':');
	if(pos == std::string::npos)
		return false;

	if(line.find(':', pos + 1) != std::string::npos)
		return false;

	key = trim(line.substr(0, pos));
	value = trim(line.substr(pos + 1));

	if(key.empty())
		return false;

	i = 0;
	while(i < key.size())
	{
		if(key[i] == ' ' || key[i] == '\t')
			return false;
		i++;
	}

	_headers[key] = value;
	return true;
}

const std::string &RequestParser::getMethod() const
{
	return _method;
}

const std::string &RequestParser::getPath() const
{
	return _path;
}

const std::string &RequestParser::getVersion() const
{
	return _version;
}

const std::map<std::string, std::string> &RequestParser::getHeaders() const
{
	return _headers;
}
