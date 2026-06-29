#include "network/Poller.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"

Poller::Poller() {}
Poller::~Poller() {}

void Poller::addFd(int fd, short events)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    _pollfds.push_back(pfd);
    
    Logger::debug(std::string("Added FD: ") + StringUtils::to_string(fd) + " to monitoring.");
}

void Poller::removeFd(int fd)
{
    for(std::vector<struct pollfd>::iterator it = _pollfds.begin(); it != _pollfds.end(); ++it)
    {
        if (it->fd == fd)
        {
            _pollfds.erase(it);
            Logger::debug(std::string("Removed FD: ") + StringUtils::to_string(fd) + " from monitoring.");
            break;
        }
    }
}

int Poller::poll (int timeout)
{
    if (_pollfds.empty())
        return 0;
    return ::poll(&_pollfds[0], _pollfds.size(), timeout);
}

std::vector<struct pollfd>& Poller::getFds()
{
    return _pollfds;
}