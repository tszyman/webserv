#ifndef EVENTLOOP_HPP
#define EVENTLOOP_HPP

#include "network/Poller.hpp"
#include "network/SocketEngine.hpp"
#include "network/Connection.hpp"
#include <map>
#include <stdexcept>
#include <sys/socket.h>

class EventLoop
{
    private:
        Poller          _poller;
        SocketEngine*   _server_engine;
        bool            _is_running;

        std::map<int, Connection*> _connections;

        EventLoop(const EventLoop& other);
        EventLoop& operator=(const EventLoop& other);

    public:
        EventLoop(SocketEngine* engine);
        ~EventLoop();
        void run();
};
#endif