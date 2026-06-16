#include "routing/Router.hpp"
#include "http/HttpErrorPage.hpp"
#include <iostream>
#include <cstdio>
#include <sys/stat.h>

Router::Router() {}

void Router::addLocation(const LocationConfig& location)
{
	_locations.push_back(location);
}

// Prefix matching
const LocationConfig* Router::matchLocation(const std::string& uri) const
{
	const LocationConfig* bestMatch = NULL;
	size_t longestMatchLength = 0;

	for (size_t i = 0; i < _locations.size() ; ++i)
	{
		const std::string& locationPath = _locations[i].getPath();

		//If the URI starts with the location path
		if (uri.find(locationPath) == 0)
		{
			if (locationPath.length() > longestMatchLength)
			{
				longestMatchLength = locationPath.length();
				bestMatch = &_locations[i];
			}
		}
	}
	return(bestMatch);
}

// Translates: URI "/whatever/what" + Location "/whatever" + Root "/tmp/www" => "/tmp/www/what"
std::string Router::translatePath(const std::string& uri, const LocationConfig* location) const
{
	if (!location)
		return(uri); //fallback if no location matched

	std::string relativePath = uri.substr(location->getPath().length());

	// Handle slashes to avoid "//" (when joining root and path)
	std::string root = location->getRoot();
	//if root ends with '/' and relativePath starts with '/', remove one to avoid "//"
	if (!root.empty() && root[root.length() - 1] == '/' && !relativePath.empty() && !relativePath[0] == '/')
	{
		root.erase(root.length() - 1);
	}
	//If neither has a '/', add one so they don't glue together
	else if (!root.empty() && root[root.length() - 1] != '/' && !relativePath.empty() && !relativePath[0] != '/')
	{
		root += "/";
	}

	return root + relativePath;
}

void Router::route(const RequestParser& request, HttpResponse& response) const
{
	const LocationConfig* location = matchLocation(request.getPath());

	// 1. HAndle 404 Not Found
	if (location == NULL)
	{
		std::cerr << "[INFO] Router: No location found for " << request.getPath() << std::endl;
		response.setStatusCode(404);
		// somehow here need to build 404 error page. Not sure if used interface properly
		response.setBody(ErrorPage::defaultBody(404));
		return;
	}

	// 2. Handle 405 Method Not Allowed
	if (!location->isMethodAllowed(request.getMethod()))
	{
		std::cerr << "[WARINING] Router: 405 Method Not Allowed (" << request.getMethod() << ") for " << request.getPath() << std::endl;
		response.setStatusCode(405);
		// somehow here need to build 405 error page. Not sure if used interface properly
		response.setBody(ErrorPage::defaultBody(405));
		return;
	}

	// 3. Translate the path and route to the correct handler
	std::string physicalPath = translatePath(request.getPath(), location);

	if (request.getMethod() == "GET"){
		handleGet(physicalPath, response);
	} else if (request.getMethod() == "POST") {
		handlePost(request, physicalPath, response);
	} else if (request.getMethod() == "DELETE"){
		handleDelete(physicalPath, response);
	} else {
		// Handle 501 Not Implemented (for methods like PUT, PATCH if not implemented)
		response.setStatusCode(501);
		response.setBody(ErrorPage::defaultBody(501));
	}
}

void Router::handleDelete(const std::string& physicalPath, HttpResponse& response) const
{
	struct stat buffer;
	// check if the file exists using stat
	if (stat(physicalPath.c_str(), &buffer) != 0)
	{
		std::cout << "[INFO] DELETE Error: File not found - " << physicalPath << std::endl;
		response.setStatusCode(404);
		response.setBody(ErrorPage::defaultBody(404));
		return;
	}

	// try to remove the file using standard C++
	if (std::remove(physicalPath.c_str()) == 0)
	{
		std::cout << "[INFO] DELETE Success: " << physicalPath << std::endl;
		response.setStatusCode(202);
		response.setBody(ErrorPage::defaultBody(204));
	}
	else
	{
		std::cout << "[INFO] DELETE Error: Forbidden/No Access - " << physicalPath << std::endl;
		response.setStatusCode(403);
		response.setBody(ErrorPage::defaultBody(403));
	}
}