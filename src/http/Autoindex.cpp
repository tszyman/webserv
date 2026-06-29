#include "../../include/http/Autoindex.hpp"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <exception>
#include <sstream>
#include <sys/stat.h>

namespace
{
    class DirGuard
    {
    public:
        explicit DirGuard(DIR *dir) : _dir(dir) {}
        ~DirGuard()
        {
            if (_dir != NULL)
                closedir(_dir);
        }

        DIR *get() const
        {
            return _dir;
        }

    private:
        DirGuard(const DirGuard &other);
        DirGuard &operator=(const DirGuard &other);

        DIR *_dir;
    };
}

std::string Autoindex::buildBody(const std::string &directoryPath, const std::string &requestUri)
{
    std::vector<Entry> entries;
    std::vector<Entry>::const_iterator it;
    const std::string displayUri = ensureTrailingSlash(requestUri);
    std::string body;

    if (!readEntries(directoryPath, entries))
        return std::string();
    std::sort(entries.begin(), entries.end(), entryLess);
    body += "<!DOCTYPE html>\n";
    body += "<html>\n";
    body += "<head><title>Index of ";
    body += htmlEscape(displayUri);
    body += "</title></head>\n";
    body += "<body>\n";
    body += "<h1>Index of ";
    body += htmlEscape(displayUri);
    body += "</h1>\n";
    body += "<hr>\n";
    body += "<pre>";
    if (displayUri != "/")
        body += "<a href=\"../\">../</a>\n";
    for (it = entries.begin(); it != entries.end(); ++it)
    {
        const std::string suffix = it->isDirectory ? "/" : "";
        const std::string visibleName = it->name + suffix;
        const std::string href = urlEncode(it->name) + suffix;

        body += "<a href=\"";
        body += href;
        body += "\">";
        body += htmlEscape(visibleName);
        body += "</a>\n";
    }
    body += "</pre>\n";
    body += "<hr>\n";
    body += "</body>\n";
    body += "</html>\n";
    return body;
}

HttpResponse Autoindex::buildResponse(const std::string &directoryPath, const std::string &requestUri)
{
    const std::string body = buildBody(directoryPath, requestUri);
    HttpResponse response(200, body);

    response.setHeader("Content-Type", "text/html");
    response.setHeader("Content-Length", HttpResponse::numberToString(body.size()));
    return response;
}

bool Autoindex::tryBuildResponse(const std::string &directoryPath, const std::string &requestUri, HttpResponse &response)
{
    try
    {
        if (!isDirectory(directoryPath))
            return false;
        response = buildResponse(directoryPath, requestUri);
        return !response.getBody().empty();
    }
    catch(const std::exception &){}
    catch(...){}
    return false;   
}

bool Autoindex::readEntries(const std::string &directoryPath, std::vector<Entry> &entries)
{
    DIR *rawDir = opendir(directoryPath.c_str());
    struct dirent *dirEntry;

    if (rawDir == NULL)
        return false;
    DirGuard dir(rawDir);
    while ((dirEntry = readdir(dir.get())) != NULL)
    {
        const std::string name = dirEntry->d_name;
        Entry entry;

        if (name == "." || name == "..")
            continue;
        entry.name = name;
        entry.isDirectory = isDirectory(joinPath(directoryPath, name));
        entries.push_back(entry);
    }
    return true;
}

bool Autoindex::isDirectory(const std::string &path)
{
    struct stat info;

    if (stat(path.c_str(), &info) != 0)
        return false;
    return S_ISDIR(info.st_mode);
}

std::string Autoindex::joinPath(const std::string &directoryPath, const std::string &entryName)
{
    if (directoryPath.empty() || directoryPath == "/")
        return "/" + entryName;
    if (directoryPath[directoryPath.size() - 1] == '/')
        return directoryPath + entryName;
    return directoryPath + "/" + entryName;
}

std::string Autoindex::ensureTrailingSlash(const std::string &uri)
{
    if (uri.empty())
        return "/";
    if (uri[uri.size() - 1] == '/')
        return uri;
    return uri + "/";
}

std::string Autoindex::htmlEscape(const std::string &value)
{
    std::string result;
    std::string::const_iterator it;

    for (it = value.begin(); it != value.end(); ++it)
    {
        if (*it == '&')
            result += "&amp;";
        else if (*it == '<')
            result += "&lt;";
        else if (*it == '>')
            result += "&gt;";
        else if (*it == '"')
            result += "&quot;";
        else
            result += *it;
    }
    return result;
}

std::string Autoindex::urlEncode(const std::string &value)
{
    static const char *hex = "0123456789ABCDEF";
    std::string result;
    std::string::const_iterator it;

    for (it = value.begin(); it != value.end(); ++it)
    {
        const unsigned char c = static_cast<unsigned char>(*it);

        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            result += static_cast<char>(c);
        else
        {
            result += '%';
            result += hex[c >> 4];
            result += hex[c & 15];
        }
    }
    return result;
}

bool Autoindex::entryLess(const Entry &left, const Entry &right)
{
    if (left.isDirectory != right.isDirectory)
        return left.isDirectory;
    return left.name < right.name;
}