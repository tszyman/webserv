#ifndef CGI_PROCESS_HPP
#define CGI_PROCESS_HPP

#include <string>
#include <sys/types.h> //for pid_t

class CgiProcess {
	public:
		CgiProcess();
		~CgiProcess();

		bool execute(const std::string& scriptPath, const std::string& cgiExecutable, char** envp);
		//Getters for the EventLoop / Poller to regiser these FDs
		int getServerToCgiFd() const;
		int getCgiToServerFd() const;
		pid_t getPid() const;

	private:
		int _serverToCgiFd; // Server writes to this FD (CGI's stdin)
		int _cgiToServerFd; // Server reads from this FD (CGI's stdout)
		pid_t _pid;
};

#endif