#ifndef CGI_ENV_HPP
#define CGI_ENV_HPP

#include <map>
#include <string>
#include "parser/RequestParser.hpp"

class CgiEnv {
	public:
		CgiEnv(const RequestParser& request, const std::string& scriptPath);
		CgiEnv(const CgiEnv& other);
		CgiEnv operator=(const CgiEnv& other);
		~CgiEnv();

		char** getEnv() const;
	
	private:
		std::map<std::string, std::string> _envMap;
		char** _envp;

		void _buildEnvpArray();
};

#endif