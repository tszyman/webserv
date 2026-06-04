#include "../../include/http/HttpErrorPage.hpp"
#include "../../include/http/StatusCodes.hpp"

#include <exception>
#include <string>

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

HttpResponse ErrorPage::buildDefault(int statusCode)
{
    const int normalizedStatus = normalizedErrorStatus(statusCode);
    const std::string body = defaultBody(normalizedStatus);
    HttpResponse response(normalizedStatus, body);

    response.setHeader("Content-Type", "text/html");
    response.setHeader("Content-Length", HttpResponse::numberToString(body.size()));
    response.setConnectionClose(true);
    return response;
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