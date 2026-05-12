#ifndef SERVER_HPP
#define SERVER_HPP

class Server
{
    public:
        Server();
        bool loadConfig(int argc, char **argv);
        void run();
};

#endif