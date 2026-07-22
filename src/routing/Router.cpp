#include "routing/Router.hpp"
#include "http/Autoindex.hpp"
#include "http/HttpErrorPage.hpp"
#include "http/UploadHandler.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <sys/stat.h>

static bool isLocationPrefix(const std::string& uri, const std::string& locationPath)
{
	if (uri.compare(0, locationPath.size(), locationPath) != 0)
		return false;
	if (locationPath == "/" || uri.size() == locationPath.size())
		return true;
	return locationPath[locationPath.size() - 1] == '/' || uri[locationPath.size()] == '/';
}

static bool containsTraversal(const std::string& path)
{
	std::string::size_type start = 0;
	while (start <= path.size())
	{
		const std::string::size_type end = path.find('/', start);
		const std::string part = path.substr(start, end - start);
		if (part == "..")
			return true;
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	return false;
}

static bool extractBoundary(const std::string& contentType, std::string& boundary)
{
	const std::string marker = "boundary=";
	const std::string::size_type markerPos = contentType.find(marker);
	if (contentType.compare(0, 19, "multipart/form-data") != 0 || markerPos == std::string::npos)
		return false;
	boundary = contentType.substr(markerPos + marker.size());
	const std::string::size_type semicolon = boundary.find(';');
	if (semicolon != std::string::npos)
		boundary.erase(semicolon);
	if (boundary.size() >= 2 && boundary[0] == '"' && boundary[boundary.size() - 1] == '"')
		boundary = boundary.substr(1, boundary.size() - 2);
	return !boundary.empty() && boundary.find('\r') == std::string::npos && boundary.find('\n') == std::string::npos;
}

static bool extractDispositionValue(const std::string& disposition, const std::string& key, std::string& value)
{
	const std::string marker = key + "=";
	const std::string::size_type pos = disposition.find(marker);
	if (pos == std::string::npos)
		return false;
	const std::string::size_type begin = pos + marker.size();
	if (begin >= disposition.size() || disposition[begin] != '"')
		return false;
	const std::string::size_type end = disposition.find('"', begin + 1);
	if (end == std::string::npos)
		return false;
	value = disposition.substr(begin + 1, end - begin - 1);
	return true;
}

static bool parseMultipartFile(const std::string& body, const std::string& boundary, std::string& filename, std::string& fileBody)
{
	const std::string opening = "--" + boundary + "\r\n";
	const std::string separator = "\r\n\r\n";
	const std::string closingPrefix = "\r\n--" + boundary;
	if (body.compare(0, opening.size(), opening) != 0)
		return false;
	const std::string::size_type headersEnd = body.find(separator, opening.size());
	if (headersEnd == std::string::npos)
		return false;
	const std::string headers = body.substr(opening.size(), headersEnd - opening.size());
	const std::string dispositionName = "Content-Disposition:";
	const std::string::size_type dispositionPos = headers.find(dispositionName);
	if (dispositionPos == std::string::npos)
		return false;
	const std::string::size_type dispositionEnd = headers.find("\r\n", dispositionPos);
	const std::string disposition = headers.substr(dispositionPos + dispositionName.size(), dispositionEnd - dispositionPos - dispositionName.size());
	if (!extractDispositionValue(disposition, "filename", filename))
		return false;
	const std::string::size_type dataStart = headersEnd + separator.size();
	const std::string::size_type dataEnd = body.find(closingPrefix, dataStart);
	if (dataEnd == std::string::npos || body.compare(dataEnd + closingPrefix.size(), 2, "--") != 0)
		return false;
	fileBody.assign(body, dataStart, dataEnd - dataStart);
	return true;
}

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
		if (isLocationPrefix(uri, locationPath))
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
	if (!root.empty() && root[root.length() - 1] == '/' && !relativePath.empty() && relativePath[0] == '/')
	{
		root.erase(root.length() - 1);
	}
	//If neither has a '/', add one so they don't glue together
	else if (!root.empty() && root[root.length() - 1] != '/' && !relativePath.empty() && relativePath[0] != '/')
	{
		root += "/";
	}

	return root + relativePath;
}

void Router::route(const RequestParser& request, HttpResponse& response) const
{
	if (request.getState() == RequestParser::STATE_PAYLOAD_TOO_LARGE)
	{
		Logger::warning("Router: Payload too large.");
		response.setStatusCode(413);
		response.setBody(ErrorPage::defaultBody(413));
		return;
	}

	if (request.getMethodState() == RequestParser::METHOD_UNKNOWN)
	{
		Logger::warning("Router: 400 Bad Request (unknown method) for " + request.getPath());
		response = ErrorPage::buildDefault(400);
		return;
	}

	if (request.getMethodState() == RequestParser::METHOD_UNIMPLEMENTED)
	{
		Logger::warning("Router: 501 Not Implemented (" + request.getMethod() + ") for " + request.getPath());
		response = ErrorPage::buildDefault(501);
		return;
	}

	if (containsTraversal(request.getPath()))
	{
		Logger::warning("Router: rejected path traversal attempt: " + request.getPath());
		response = ErrorPage::buildDefault(403);
		return;
	}

	const LocationConfig* location = matchLocation(request.getPath());

	// 1. HAndle 404 Not Found
	if (location == NULL)
	{
		Logger::info("Router: No location found for " + request.getPath());
		response.setStatusCode(404);
		// somehow here need to build 404 error page. Not sure if used interface properly
		response.setBody(ErrorPage::defaultBody(404));
		return;
	}

	// 2. Handle 405 Method Not Allowed
	if (!location->isMethodAllowed(request.getMethod()))
	{
		Logger::warning("Router: 405 Method Not Allowed (" + request.getMethod() + ") for " + request.getPath());
		response.setStatusCode(405);
		// somehow here need to build 405 error page. Not sure if used interface properly
		response.setBody(ErrorPage::defaultBody(405));
		return;
	}

	// 3. Translate the path and route to the correct handler
	std::string physicalPath = translatePath(request.getPath(), location);

	if (request.getMethod() == "GET"){
		handleGet(request.getPath(), physicalPath, location, response);
	} else if (request.getMethod() == "POST") {
		handlePost(request, location, response);
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
		Logger::info("DELETE Error: File not found - " + physicalPath);
		response.setStatusCode(404);
		response.setBody(ErrorPage::defaultBody(404));
		return;
	}

	// try to remove the file using standard C++
	if (std::remove(physicalPath.c_str()) == 0)
	{
		Logger::info("DELETE Success: " +  physicalPath);
		response.setStatusCode(204);
		response.setBody("");
	}
	else
	{
		Logger::info("[INFO] DELETE Error: Forbidden/No Access - " + physicalPath);
		response.setStatusCode(403);
		response.setBody(ErrorPage::defaultBody(403));
	}
}

void Router::handleGet(const std::string& requestUri, const std::string& physicalPath, const LocationConfig* location, HttpResponse& response) const
{
	Logger::info("GET request for: " + physicalPath);
	std::string targetPath = physicalPath;
	std::string indexPath;
	struct stat pathStat;
	struct stat indexStat;
	bool requestIsDirectory = false;
	bool hasIndexFile = false;

	if (stat(targetPath.c_str(), &pathStat) != 0)
	{
		Logger::warning("GET eRROR: File not found - " + targetPath);
		response.setStatusCode(404);
		response.setBody(ErrorPage::defaultBody(404));
		return;
	}

	requestIsDirectory = S_ISDIR(pathStat.st_mode);
	if (requestIsDirectory)
	{
		indexPath = targetPath;
		if (indexPath[indexPath.length() - 1] != '/')
			indexPath += "/";
		indexPath += "index.html";
		hasIndexFile = (stat(indexPath.c_str(), &indexStat) == 0 && !S_ISDIR(indexStat.st_mode));
		if (hasIndexFile)
		{
			targetPath = indexPath;
			pathStat = indexStat;
		}
		else if (location != NULL && location->getAutoindex())
		{
			if (Autoindex::tryBuildResponse(physicalPath, requestUri, response))
			{
				Logger::info("GET Success: Autoindex generated for " + physicalPath);
				return;
			}
			Logger::warning("GET Error: Failed to generate autoindex for " + physicalPath);
			response = ErrorPage::buildDefault(500);
			return;
		}
		else
		{
			Logger::warning("GET Error: Directory listing is disabled - " + physicalPath);
			response = ErrorPage::buildDefault(403);
			return;
		}
	}

	if (stat(targetPath.c_str(), &pathStat) != 0 || S_ISDIR(pathStat.st_mode))
	{
		Logger::warning("GET Error: Index file not found in directory - " + targetPath);
		response.setStatusCode(403);
		response.setBody(ErrorPage::defaultBody(403));
		return;
	}
	// check permission for read
	if (!(pathStat.st_mode & S_IRUSR))
	{
		Logger::warning("GET Error: Permission denied - " + targetPath);
		response.setStatusCode(403);
		response.setBody(ErrorPage::defaultBody(403));
		return;
	}

	std::ifstream file(targetPath.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open())
	{
		Logger::error("GET Error: Could not open file - " + targetPath);
		response.setStatusCode(500);
		response.setBody(ErrorPage::defaultBody(500));
		return;
	}

	std::ostringstream ss;
	ss << file.rdbuf();
	file.close();

	response.setStatusCode(200);
	response.setHeader("Content-Length", StringUtils::to_string(ss.str().length()));
	response.setHeader("Content-Type", getMimeType(targetPath));
	response.setBody(ss.str());
	Logger::info("GET Success: Loaded " + StringUtils::to_string(ss.str().length()) + " bytes from " + targetPath);
}

void Router::handlePost(const RequestParser& request, const LocationConfig* location, HttpResponse& response) const
{
	if (location != NULL && location->isUploadEnabled())
	{
		const std::map<std::string, std::string>& headers = request.getHeaders();
		std::map<std::string, std::string>::const_iterator contentType = headers.find("Content-Type");
		std::string boundary;
		std::string filename;
		std::string fileBody;
		const std::vector<char>& rawBody = request.getBody();
		const std::string body(rawBody.begin(), rawBody.end());

		if (contentType == headers.end() || !extractBoundary(contentType->second, boundary)
			|| !parseMultipartFile(body, boundary, filename, fileBody))
		{
			response = ErrorPage::buildDefault(400);
			return;
		}
		UploadHandler::handleUpload(location->getUploadStore(), filename, fileBody, 0, response);
		return;
	}

	// Preserve the previous generic POST behaviour for non-upload endpoints.
	response.setStatusCode(200);
	response.setBody(ErrorPage::defaultBody(200));
}

std::string Router::getMimeType(const std::string& path) const
{
	size_t dotPos = path.find_last_of('.');
	if (dotPos == std::string::npos)
		return "application/octet-stream";
	std::string ext = path.substr(dotPos);

	if (ext == ".html" || ext == ".htm") return "text/html";
	if (ext == ".css") return "text/css";
	if (ext == ".js") return "application/javascript";
	if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
	if (ext == ".png") return "image/png";
	if (ext == ".gif") return "image/gif";
	if (ext == ".ico") return "image/x-icon";
	if (ext == ".txt") return "text/plain";
	if (ext == ".json") return "application/json";
	if (ext == ".pdf") return "application/pdf";

	return "application/octet-stream";
}
