#include "../../include/http/HttpResponse.hpp"
#include "../../include/http/StatusCodes.hpp"
#include "filesystem/FileSystem.hpp"
#include "http/HttpErrorPage.hpp"
#include <sstream>

HttpResponse::HttpResponse()
    : _statusCode(200), _headers(), _body(), _closeConnection(false){}

HttpResponse::HttpResponse(int statusCode, const std::string &body)
    : _statusCode(statusCode), _headers(), _body(body), _closeConnection(false){}

HttpResponse::~HttpResponse(){}

int HttpResponse::getStatusCode() const
{
    return _statusCode;
}

const std::string &HttpResponse::getBody() const
{
    return _body;
}

const HttpResponse::HeaderMap &HttpResponse::getHeaders() const
{
    return _headers;
}

void HttpResponse::setStatusCode(int statusCode)
{
    _statusCode = statusCode;
}

void HttpResponse::setBody(const std::string &body)
{
    _body = body;
}

void HttpResponse::setHeader(const std::string &name, const std::string &value)
{
    _headers[name] = value;
}

void HttpResponse::setConnectionClose(bool closeConnection)
{
    _closeConnection = closeConnection;
}

std::string HttpResponse::toString() const
{
    std::ostringstream output;
    HeaderMap::const_iterator it;

    bool bodyAllowed = !(_statusCode >= 100 && _statusCode < 200) && _statusCode != 204 && _statusCode != 304;

    output << "HTTP/1.1 " << _statusCode << " "
           << HttpStatus::reasonPhrase(_statusCode) << "\r\n";
    for (it = _headers.begin(); it != _headers.end(); ++it)
    {
        if (it->first == "Content-Length")
            continue;
        output << it->first << ": " << it->second << "\r\n";
    }
    if (bodyAllowed)
        output << "Content-Length: " << _body.size() << "\r\n";
    if (_closeConnection)
        output << "Connection: close\r\n";
    output << "\r\n";

    if (bodyAllowed)
        output << _body;

    return output.str(); 
}

std::string HttpResponse::numberToString(unsigned long value)
{
    std::ostringstream output;
    output << value;
    return output.str();
}

void HttpResponse::serveStaticFile(const std::string& filePath)
{
    try
    {
        std::string content = FileSystem::readFile(filePath);
        setStatusCode(200);
        setBody(content);
        setHeader("Content-Type", FileSystem::getMimeType(filePath));
        setHeader("Content-Length", numberToString(content.length()));
    }
    catch(const std::exception& e)
    {
        ErrorPage::tryBuildDefault(404, *this);
    }
}