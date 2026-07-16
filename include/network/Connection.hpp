#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h>
#include <string>
#include <ctime>
#include "parser/RequestParser.hpp"

class Connection // class representing client
{
    private:
        int _fd;
        std::string _response_buffer;
        RequestParser _parser;
        time_t _last_activity;

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
        void reset();
        void updateLastActivity();
        bool isTimedOut(int timeout_second) const;
};

#endif