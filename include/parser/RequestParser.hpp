#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP
#include <string>
#include <map>

class RequestParser
{
	public:
		RequestParser();

		bool parseRequestLine(const std::string &line);
		bool parseHeaderLine(const std::string &line);

		const std::string &getMethod() const;
		const std::string &getPath() const;
		const std::string &getVersion() const;
		const std::map<std::string, std::string> &getHeaders() const;

	private:
		std::string _method;
		std::string _path;
		std::string _version;

		std::map<std::string, std::string> _headers;

		std::string trim(const std::string &str);
};

#endif