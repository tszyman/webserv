#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include "network/Connection.hpp"

class Server
{
    private:
        std::map<int, Connection*> _client_connections;

        Server(const Server& other);
        Server& operator=(const Server& other);

    public:
        Server();
        ~Server();
        bool loadConfig(int argc, char **argv);
        void run();
        void addConnection(Connection* conn);
        void cleanupConnection(int fd);
};

#endif