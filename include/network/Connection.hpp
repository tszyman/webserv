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
		std::string _request_buffer;
        RequestParser _parser;
        time_t _last_activity;
        size_t _max_body_size;

        // Block copy (cannot exist the same FD)
        Connection(const Connection& other);
        Connection& operator=(const Connection& other);

    public:
        Connection(int fd, size_t maxBodySize = 1048576);
        ~Connection();

        int getFd() const;
        RequestParser& getParser() { return _parser; }

        void appendResponse(const std::string& data);
        std::string& getResponseBuffer();
        void eraseSentData(size_t bytes);
		void appendRequestData(const char* data, size_t bytes);
		bool hasRequestData() const;
		void parseRequestData();
        void reset();
        void updateLastActivity();
        bool isTimedOut(int timeout_second) const;
};

#endif
