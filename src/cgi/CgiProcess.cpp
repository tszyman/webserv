#include "cgi/CgiProcess.hpp"
#include <iostream>
// C++ Standard Library wrappers for C functions
#include <cstdlib>
#include <cstring>
// POSIX OS APIs (no C++ equvalent)
#include <unistd.h> // for pipe, fork, execve, close, dup2
#include <fcntl.h> // for fcntl, 0_NONBLOCK, FD_CLOEXEC
#include <sys/types.h> // for pid_t
#include <vector>

static bool isAbsolutePath(const std::string& path)
{
	return !path.empty() && path[0] == '/';
}

static void appendPathParts(const std::string& path, std::vector<std::string>& parts)
{
	std::string::size_type start = 0;
	while (start < path.size())
	{
		std::string::size_type end = path.find('/', start);
		std::string part = path.substr(start, end == std::string::npos
			? std::string::npos : end - start);
		if (!part.empty() && part != ".")
		{
			if (part == ".." && !parts.empty() && parts.back() != "..")
				parts.pop_back();
			else
				parts.push_back(part);
		}
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
}

static std::string makeExecutablePathFromScriptDirectory(const std::string& scriptDirectory,
	const std::string& executablePath)
{
	if (isAbsolutePath(executablePath) || scriptDirectory.empty() || scriptDirectory == ".")
		return executablePath;

	std::vector<std::string> scriptParts;
	std::vector<std::string> executableParts;
	appendPathParts(scriptDirectory, scriptParts);
	appendPathParts(executablePath, executableParts);

	size_t common = 0;
	while (common < scriptParts.size() && common < executableParts.size()
		&& scriptParts[common] == executableParts[common])
		++common;

	std::string result;
	for (size_t i = common; i < scriptParts.size(); ++i)
		result += "../";
	for (size_t i = common; i < executableParts.size(); ++i)
	{
		if (!result.empty() && result[result.size() - 1] != '/')
			result += "/";
		result += executableParts[i];
	}
	return result.empty() ? "." : result;
}

CgiProcess::CgiProcess() : _serverToCgiFd(-1), _cgiToServerFd(-1), _pid(-1) {}
CgiProcess::~CgiProcess() {}

bool CgiProcess::execute(const std::string& scriptPath, const std::string& cgiExecutable, char** envp) {
	int pipe_in[2];
	int pipe_out[2];
	const std::string::size_type lastSlash = scriptPath.find_last_of('/');
	const std::string scriptDirectory = lastSlash == std::string::npos
		? "." : scriptPath.substr(0, lastSlash);
	const std::string scriptArgument = lastSlash == std::string::npos
		? scriptPath : scriptPath.substr(lastSlash + 1);
	if (isAbsolutePath(scriptDirectory) && !isAbsolutePath(cgiExecutable))
	{
		std::cerr << "CGI executable must be absolute when the script path is absolute" << std::endl;
		return false;
	}
	const std::string executablePath = makeExecutablePathFromScriptDirectory(
		scriptDirectory, cgiExecutable);

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
		// execve does not modify argv strings.  The std::string objects remain
		// alive until execve replaces this child process.
		args[0] = const_cast<char*>(executablePath.c_str());
		args[1] = const_cast<char*>(scriptArgument.c_str());
		args[2] = NULL;

		if (scriptDirectory != "." && chdir(scriptDirectory.c_str()) != 0)
		{
			std::cerr << "Failed to enter CGI directory" << std::endl;
			std::exit(1);
		}

		execve(args[0], args, envp);

		std::cerr << "Execve failed" << std::endl;
		std::exit(1);

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
