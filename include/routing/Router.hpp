#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "parser/RequestParser.hpp"
#include "http/HttpResponse.hpp"
#include "routing/LocationConfig.hpp"
#include <string>
#include <vector>

class Router {
	public:
		Router(size_t serverMaxBodySize = 0);

		void addLocation(const LocationConfig& location);

		void route(const RequestParser& request, HttpResponse& response) const;
		bool getCgiTarget(const RequestParser& request, std::string& scriptPath, std::string& executable) const;

	private:
		std::vector<LocationConfig> _locations;
		size_t _serverMaxBodySize;
		const LocationConfig* matchLocation(const std::string& uri) const;
		std::string translatePath(const std::string& uri, const LocationConfig* location) const;

		// method handlers
		void handleGet(const std::string& requestUri, const std::string& physicalPath, const LocationConfig* location, HttpResponse& response) const;
		void handlePost(const RequestParser& request, const LocationConfig* location, HttpResponse& response) const;
		void handleDelete(const std::string& physicalPath, HttpResponse& response) const;
		std::string getMimeType(const std::string& path) const;

};

#endif // ROUTER_HPP
