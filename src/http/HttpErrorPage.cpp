#include "../../include/http/HttpErrorPage.hpp"
#include "../../include/http/StatusCodes.hpp"

#include <exception>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
    class FdGuard
    {
        public:
            explicit FdGuard(int fd) : _fd(fd) {}
            ~FdGuard()
            {
                if (_fd >= 0)
                    close(_fd);
            }

        private:
            FdGuard(const FdGuard &other);
            FdGuard &operator=(const FdGuard &other);

            int _fd;
    };
}

int ErrorPage::normalizedErrorStatus(int statusCode)
{
    if (!HttpStatus::isError(statusCode) || !HttpStatus::isKnown(statusCode))
        return 500;
    return statusCode;
}

std::string ErrorPage::defaultBody(int statusCode)
{
    const int normalizedStatus = normalizedErrorStatus(statusCode);
    const std::string code = HttpResponse::numberToString(normalizedStatus);
    const std::string reason = HttpStatus::reasonPhrase(normalizedStatus);
    std::string body;

    body.reserve(160 + code.size() + reason.size() * 2);
    body += "<!DOCTYPE html>\n";
    body += "<html>\n";
    body += "<head><title>";
    body += code;
    body += " ";
    body += reason;
    body += "</title></head>\n";
    body += "<body>\n";
    body += "<center><h1>";
    body += code;
    body += " ";
    body += reason;
    body += "</h1></center>\n";
    body += "<hr><center>webserv</center>\n";
    body += "</body>\n";
    body += "</html>\n";
    return body;
}

HttpResponse ErrorPage::buildFromBody(int statusCode, const std::string &body, const std::string &contentType)
{
    HttpResponse response(statusCode, body);

    response.setHeader("Content-Type", contentType);
    response.setHeader("Content-Length", HttpResponse::numberToString(body.size()));
    response.setConnectionClose(true);
    return response;
}

HttpResponse ErrorPage::buildDefault(int statusCode)
{
    const int normalizedStatus = normalizedErrorStatus(statusCode);
    const std::string body = defaultBody(normalizedStatus);

    return buildFromBody(normalizedStatus, body, "text/html");
}

HttpResponse ErrorPage::build(int statusCode, const CustomPageMap &customPages)
{
    const int normalizedStatus = normalizedErrorStatus(statusCode);
    CustomPageMap::const_iterator it = customPages.find(normalizedStatus);
    std::string body;

    if(it != customPages.end() && readRegularFile(it->second, body))
        return buildFromBody(normalizedStatus, body, "text/html");
    return buildDefault(normalizedStatus);
}

bool ErrorPage::tryBuildDefault(int statusCode, HttpResponse &response)
{
    try
    {
        response = buildDefault(statusCode);
        return true;
    }
    catch (const std::exception &){}
    catch (...){}
    try
    {
        const std::string fallbackBody = "Internal Server Error\n";

        response.setStatusCode(500);
        response.setBody(fallbackBody);
        response.setHeader("Content-Type", "text/plain");
        response.setHeader("Content-Length", HttpResponse::numberToString(fallbackBody.size()));
        response.setConnectionClose(true);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ErrorPage::tryBuild(int statusCode, const CustomPageMap &customPages, HttpResponse &response)
{
    try
    {
        response = build(statusCode, customPages);
        return true;
    }
    catch (const std::exception &){}
    catch (...){}
    return tryBuildDefault(statusCode, response);    
}

bool ErrorPage::readRegularFile(const std::string &path, std::string &body)
{
    struct stat info;
    int         fd;
    char        buffer[4096];
    ssize_t     bytesRead;

    if (path.empty())
        return false;
    if (stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode))
        return false;
    body.clear();
    if (info.st_size > 0)
        body.reserve(static_cast<std::string::size_type>(info.st_size));
    fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return false;
    FdGuard guard(fd);
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
        body.append(buffer, static_cast<std::string::size_type>(bytesRead));
    if (bytesRead < 0)
    {
        body.clear();
        return false;
    }
    return true;
}