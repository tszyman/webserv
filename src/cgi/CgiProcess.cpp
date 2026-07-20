#include "cgi/CgiProcess.hpp"
#include <iostream>
#include <cstdlib>
// C++ Standard Library wrappers for C functions
// POSIX OS APIs (no C++ equvalent)
#include <unistd.h> // for pipe, fork, execve, close, dup2
#include <fcntl.h> // for fcntl, 0_NONBLOCK, FD_CLOEXEC
#include <sys/types.h> // for pid_t

CgiProcess::CgiProcess() : _serverToCgiFd(-1), _cgiToServerFd(-1), _pid(-1) {}
CgiProcess::~CgiProcess() {}

bool CgiProcess::execute(const std::string& scriptPath, const std::string& cgiExecutable, char** envp) {
	int pipe_in[2];
	int pipe_out[2];

	if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
	{
		std::cerr << "Pipe creation failed." << std::endl;
		return false;
	}

	_pid = fork();
	if (_pid < 0)
	{
		std::cerr << "Fork failed" << std::endl;
		return false;
	}

	if (_pid == 0){
		// CHILD PROCESS
		close(pipe_in[1]);
		close(pipe_out[0]);

		dup2(pipe_in[0], STDIN_FILENO);
		close(pipe_in[0]);
		dup2(pipe_out[1], STDOUT_FILENO);
		close(pipe_out[1]);

		char* args[3];
		args[0] = const_cast<char*>(cgiExecutable.c_str());
		args[1] = const_cast<char*>(scriptPath.c_str());
		args[2] = NULL;

		execve(args[0], args, envp);

		std::cerr << "Execve failed" << std::endl;
		exit(1);
		return false;

	} else {
		// PARENT PROCESS
		close(pipe_in[0]);
		close(pipe_out[1]);

		_serverToCgiFd = pipe_in[1];
		_cgiToServerFd = pipe_out[0];

		// Ensuring non-blocking mode
		fcntl(_serverToCgiFd, F_SETFL, O_NONBLOCK, FD_CLOEXEC);
		fcntl(_cgiToServerFd, F_SETFL, O_NONBLOCK, FD_CLOEXEC);
		
		return true;
	}
}

int CgiProcess::getServerToCgiFd() const {
	return _serverToCgiFd;
}

int CgiProcess::getCgiToServerFd() const {
	return _cgiToServerFd;
}

pid_t CgiProcess::getPid() const {
	return _pid;
}
