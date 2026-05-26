#ifndef ERROR_PAGE_HPP
# define ERROR_PAGE_HPP

# include "HttpResponse.hpp"
# include <string>

class ErrorPage
{
    public:
        static std::string defaultBody(int statusCode);
        static HttpResponse buildDefault(int statusCode);
        static bool tryBuildDefault(int statusCode, HttpResponse &response);

    private:
        ErrorPage();
        ErrorPage(const ErrorPage &other);
        ErrorPage &operator=(const ErrorPage &other);
        ~ErrorPage();

        static int normalizedErrorStatus(int statusCode);
};

#endif