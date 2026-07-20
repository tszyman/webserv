#include "core/Server.hpp"
#include "utils/Logger.hpp"
#include "network/EventLoop.hpp"
#include <csignal>

void handle_sigint(int sig)
{
    (void)sig;
    Logger::info("Signal received. Initiating graceful shutdown...");
    EventLoop::is_running = false;
}
int main(int argc, char **argv)
{
    signal(SIGINT, handle_sigint);
    signal(SIGQUIT, handle_sigint);

    Server server;

    if (!server.loadConfig(argc, argv))
    {
        return 1;
    }

        server.run();
        return 0;
}