#include "cgi/CgiHandler.hpp"

CgiHandler::CgiHandler() : _env(NULL), _process(NULL) {}
CgiHandler::~CgiHandler() {
	delete _env;
	delete _process;
}

bool CgiHandler::init(const RequestParser& request, const std::string& scriptPath, const std::string& cgiExecutable) {
	_env = new CgiEnv(request, scriptPath);
	_process = new CgiProcess();

	return _process->execute(scriptPath, cgiExecutable, _env->getEnv());
}

int CgiHandler::getWriteFd() const {
	if (_process)
		return _process->getServerToCgiFd();
	else
		return -1;
}

int CgiHandler::getReadFd() const {
	if (_process)
		return _process->getCgiToServerFd();
	else
		return -1;
}

pid_t CgiHandler::getPid() const {
	if (_process)
		return _process->getPid();
	else
		return -1;
}