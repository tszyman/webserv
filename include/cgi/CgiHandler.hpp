#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include "cgi/CgiEnv.hpp"
#include "cgi/CgiProcess.hpp"
#include "parser/RequestParser.hpp"
#include <string>

class CgiHandler {
	public:
		CgiHandler();
		~CgiHandler();

		// Bartek should call this when he detects a CGI script needs to run.
		// It returns true if fork/execve succeeded
		bool init(const RequestParser& request, const std::string& scriptPath, const std::string& cgiExecutable);

		// Bartek should use these to register with his Poller
		int getWriteFd() const;		// Pipe going INTO the CGI
		int getReadFd() const;		// Pipe going OUT of the CGI
		pid_t getPid() const;		// To reap the zombie process later
	
	private:
		CgiEnv* _env;
		CgiProcess* _process;
};

#endif