#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP
#include <string>

class RequestParser
{
    public:
        RequestParser();
        void feed(const std::string &data);
        bool parseRequestLine(const std::string &line);
		bool parseRequestMap(const std::string &line);

        bool isComplete() const;
        const std::string &getMethod() const;
        const std::string &getPath() const;
        const std::string &getVersion() const;
    
    private:
        std::string _method;
        std::string _path;
        std::string _version;
};

#endif