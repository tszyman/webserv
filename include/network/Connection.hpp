#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h>
#include <string>

class Connection // class representing client
{
    private:
        int _fd;
        std::string _response_buffer;

        // Block copy (cannot exist the same FD)
        Connection(const Connection& other);
        Connection& operator=(const Connection& other);

    public:
        Connection(int fd);
        ~Connection();

        int getFd() const;

        void appendResponse(const std::string& data);
        std::string& getResponseBuffer();
        void eraseSentData(size_t bytes);
};

#endif