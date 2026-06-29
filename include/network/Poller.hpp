#ifndef POLLER_HPP
#define POLLER_HPP

#include <vector>
#include <poll.h>
#include <iostream>

class Poller
{
    private:
            std::vector<struct pollfd> _pollfds;
            Poller(const Poller& other);
            Poller& operator=(const Poller& other);

    public:
            Poller();
            ~Poller();

            void addFd(int fd, short events);
            void removeFd(int fd);

            int poll (int timeout);

            std::vector<struct pollfd>& getFds();
};
#endif