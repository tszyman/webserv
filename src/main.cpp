#include "core/Server.hpp"

int main(int argc, char **argv)
{
    Server server;

    if (!server.loadConfig(argc, argv))
    {
        return 1;
    }

        server.run();
        return 0;
}