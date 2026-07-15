#ifndef EVENTLOOP_HPP
#define EVENTLOOP_HPP

#include "network/Poller.hpp"
#include "network/SocketEngine.hpp"
#include "network/Connection.hpp"
#include <map>
#include <stdexcept>
#include <sys/socket.h>
#include <vector>

class EventLoop
{
    private:
        Poller          _poller;
        bool            _is_running;

        std::map<int, Connection*> _connections;
        std::vector<SocketEngine*> _server_engines;

        EventLoop(const EventLoop& other);
        EventLoop& operator=(const EventLoop& other);

    public:
        EventLoop(const std::vector<SocketEngine*>& engines);
        ~EventLoop();
        void run();
};
#endif