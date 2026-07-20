#ifndef EVENTLOOP_HPP
#define EVENTLOOP_HPP

#include "network/Poller.hpp"
#include "network/SocketEngine.hpp"
#include "network/Connection.hpp"
#include "core/Config.hpp"
#include "routing/Router.hpp"
#include "cgi/CgiHandler.hpp"
#include <map>
#include <stdexcept>
#include <sys/socket.h>
#include <vector>

class EventLoop
{
    private:
		struct CgiSession
		{
			int clientFd;
			CgiHandler *handler;
			std::string input;
			std::string output;
			size_t inputOffset;
			bool writeClosed;
			bool readClosed;
			CgiSession() : clientFd(-1), handler(NULL), inputOffset(0), writeClosed(false), readClosed(false) {}
		};
        Poller          _poller;
        bool            _is_running;

        std::map<int, Connection*> _connections;
        std::vector<SocketEngine*> _server_engines;
        std::vector<ServerConfig>   _servers;
		std::map<int, CgiSession*> _cgiByFd;
		std::map<int, CgiSession*> _cgiByClient;

        EventLoop(const EventLoop& other);
        EventLoop& operator=(const EventLoop& other);
        const ServerConfig* matchServerConfig(const std::string& hostHeader) const;
        size_t getMaxBodySizeForPort(int port) const;
		void startCgi(Connection *connection, const RequestParser& request, const std::string& scriptPath, const std::string& executable);
		void handleCgiFd(int fd, short revents);
		void finishCgi(CgiSession *session);
		void destroyCgi(CgiSession *session);

    public:
        EventLoop(const std::vector<SocketEngine*>& engines, const std::vector<ServerConfig>& servers);
        ~EventLoop();
        void run();
};
#endif
