#ifndef AUTOINDEX_HPP
# define AUTOINDEX_HPP

#include "http/HttpResponse.hpp"
#include <string>
#include <vector>

class Autoindex
{
    public:
        static std::string buildBody(const std::string &directoryPath, const std::string &requestUri);
        static HttpResponse buildResponse(const std::string &directoryPath, const std::string &requestUri);
        static bool tryBuildResponse(const std::string &directoryPath, const std::string &requestUri, HttpResponse &response);
        
    private:
        struct Entry
        {
            std::string name;
            bool isDirectory;
        };
    
    Autoindex();
    Autoindex(const Autoindex &other);
    Autoindex &operator=(const Autoindex &other);
    ~Autoindex();

    static bool readEntries(const std::string &directoryPath, std::vector<Entry> &entries);
    static bool isDirectory(const std::string &path);
    static std::string joinPath(const std::string &directoryPath, const std::string &entryName);
    static std::string ensureTrailingSlash(const std::string &uri);
    static std::string htmlEscape(const std::string &value);
    static std::string urlEncode(const std::string &value);
    static bool entryLess(const Entry &left, const Entry &right);
};

#endif