#ifndef ERROR_PAGE_HPP
# define ERROR_PAGE_HPP

# include "HttpResponse.hpp"
# include <string>
# include <map>

class ErrorPage
{
    public:
        // Maps HTTP status codes to already-resolved filesystem paths from config.
        // The error page loader reads these files directly and never re-enters
        // routing, so missing custom pages safely fall back to defaults.
        typedef std::map<int, std::string> CustomPageMap;
    
        static std::string defaultBody(int statusCode);
        static HttpResponse buildDefault(int statusCode);
        static HttpResponse build(int statusCode, const CustomPageMap &customPages);
        static bool tryBuildDefault(int statusCode, HttpResponse &response);
        static bool tryBuild(int statusCode, const CustomPageMap &customPages, HttpResponse &response);

    private:
        ErrorPage();
        ErrorPage(const ErrorPage &other);
        ErrorPage &operator=(const ErrorPage &other);
        ~ErrorPage();

        static int normalizedErrorStatus(int statusCode);
        static bool readRegularFile(const std::string &path, std::string &body);
        static HttpResponse buildFromBody(int statusCode, const std::string &body, const std::string &contentType);
};

#endif