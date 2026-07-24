#include "../../include/http/StatusCodes.hpp"

const char *HttpStatus::reasonPhrase(int statusCode)
{
    switch (statusCode)
    {
        case 202: return "Accepted";
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default: return "Internal Server Error";
    }
}

bool HttpStatus::isKnown(int statusCode)
{
    switch (statusCode)
    {
        case 202:
        case 200:
        case 201:
        case 204:
        case 301:
        case 302:
        case 400:
        case 403:
        case 404:
        case 405:
        case 408:
        case 411:
        case 413:
        case 414:
        case 500:
        case 501:
        case 502:
        case 504:
        case 505:
            return true;
        default:
            return false;
    }
}

bool HttpStatus::isError(int statusCode)
{
    return statusCode >= 400 && statusCode <= 599;
}