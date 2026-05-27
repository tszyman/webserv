#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h>

class Connection // class representing client
{
    private:
        int _fd;

        // Block copy (cannot exist the same FD)
        Connection(const Connection& other);
        Connection& operator=(const Connection& other);

    public:
        Connection(int fd);
        ~Connection();

        int getFd() const;
};

#endif