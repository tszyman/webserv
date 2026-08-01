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
        struct CgiState
        {
            Connection* client;
            pid_t pid;
            int readFd;
            int writeFd;
            std::string requestBody;
            size_t requestBodyOffset;
            bool requestBodyClosed;
            bool requestBodyComplete;
            bool responseHeadersSent;
			bool outputClosed;
            std::string output;
        };

        Poller          _poller;

        std::map<int, Connection*> _connections;
        std::vector<SocketEngine*> _server_engines;
        std::vector<ServerConfig>   _servers;
        std::map<int, CgiState> _cgi_states;
        std::map<int, int> _cgi_write_to_read;

        EventLoop(const EventLoop& other);
        EventLoop& operator=(const EventLoop& other);
		const ServerConfig* matchServerConfig(const std::string& hostHeader,
			const std::string& listeningHost, int listeningPort) const;
        size_t getMaxBodySizeForEndpoint(const std::string& host, int port) const;
        void queueResponse(Connection* connection, const HttpResponse& response);
        bool hasActiveCgi(Connection* connection) const;
        void cancelCgi(Connection* connection);
		void reapFinishedCgis();
        static bool writeAll(int fd, const std::string& data);
        static bool parseCgiOutput(const std::string& rawOutput, HttpResponse& response);

    public:
        static bool is_running;
        EventLoop(const std::vector<SocketEngine*>& engines, const std::vector<ServerConfig>& servers);
        ~EventLoop();
        void run();
};
#endif
