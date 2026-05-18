#include "parser/RequestParser.hpp"
#include <string>

RequestParser::RequestParser()
{   
}

bool RequestParser::parseRequestLine(const std::string &line)
{
    std::string::size_type pos1;
    std::string::size_type pos2;

    pos1 = line.find(' ');
    if (pos1 == std::string::npos)
        return false;

    pos2 = line.find(' ', pos1 + 1);
    if (pos2 == std::string::npos)
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
