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
		std::string _listening_host;
		int _listening_port;
        time_t _last_activity;
        size_t _max_body_size;

        // Block copy (cannot exist the same FD)
        Connection(const Connection& other);
        Connection& operator=(const Connection& other);

    public:
        Connection(int fd, size_t maxBodySize = 1048576,
			const std::string& listeningHost = "0.0.0.0", int listeningPort = 0);
        ~Connection();

        int getFd() const;
		const std::string& getListeningHost() const;
		int getListeningPort() const;
        RequestParser& getParser() { return _parser; }

        void appendResponse(const std::string& data);
        std::string& getResponseBuffer();
        void eraseSentData(size_t bytes);
        void reset();
        void updateLastActivity();
        bool isTimedOut(int timeout_second) const;
};

#endif
