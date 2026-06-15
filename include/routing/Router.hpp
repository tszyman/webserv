#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "parser/RequestParser.hpp"
#include "http/HttpResponse.hpp"
#include "routing/LocationConfig.hpp"
#include <string>
#include <vector>

class Router {
	public:
		Router();

		void addLocation(const LocationConfig& location);

		void route(const RequestParser& request, HttpResponse& response) const;

	private:
		std::vector<LocationConfig> _locationConfig;
		const LocationConfig* matchLocation(const std::string& uri) const;
		std::string translatePath(const std::string& uri, LocationConfig* location) const;

		// method handlers
		void handleGet(const std::string& physicalPath, HttpResponse& response) const;
		void handlePost(const RequestParser& request, std::string& physicalPath, HttpResponse& response) const;
		void handleDelete(const std::string& physicalPath, HttpResponse& response) const;

};

#endif // ROUTER_HPP
