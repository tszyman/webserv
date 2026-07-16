#ifndef EVENTLOOP_HPP
#define EVENTLOOP_HPP

#include "network/Poller.hpp"
#include "network/SocketEngine.hpp"
#include "network/Connection.hpp"
#include "core/Config.hpp"
#include "routing/Router.hpp"
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
        std::vector<ServerConfig>   _servers;

        EventLoop(const EventLoop& other);
        EventLoop& operator=(const EventLoop& other);
        const ServerConfig* matchServerConfig(const std::string& hostHeader) const;

    public:
        EventLoop(const std::vector<SocketEngine*>& engines, const std::vector<ServerConfig>& servers);
        ~EventLoop();
        void run();
};
#endif