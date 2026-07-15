#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h>
#include <string>
#include "parser/RequestParser.hpp"

class Connection // class representing client
{
    private:
        int _fd;
        std::string _response_buffer;
        RequestParser _parser;

        // Block copy (cannot exist the same FD)
        Connection(const Connection& other);
        Connection& operator=(const Connection& other);

    public:
        Connection(int fd);
        ~Connection();

        int getFd() const;
        RequestParser& getParser() { return _parser; }

        void appendResponse(const std::string& data);
        std::string& getResponseBuffer();
        void eraseSentData(size_t bytes);
};

#endif