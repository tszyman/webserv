#include "routing/Router.hpp"
#include "cgi/CgiHandler.hpp"
#include "http/Autoindex.hpp"
#include "http/HttpErrorPage.hpp"
#include "http/UploadHandler.hpp"
#include "utils/Logger.hpp"
#include "cgi/CgiHandler.hpp"
#include "utils/StringUtils.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

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

static bool resolveIndexFile(const std::string& directoryPath, const LocationConfig* location, std::string& resolvedPath)
{
	std::vector<std::string> indexFiles;
	if (location != NULL && !location->getIndexFiles().empty())
		indexFiles = location->getIndexFiles();
	else
		indexFiles.push_back("index.html");

	for (size_t i = 0; i < indexFiles.size(); ++i)
	{
		std::string candidate = directoryPath;
		if (!candidate.empty() && candidate[candidate.length() - 1] != '/')
			candidate += "/";
		candidate += indexFiles[i];

		struct stat candidateStat;
		if (stat(candidate.c_str(), &candidateStat) == 0 && !S_ISDIR(candidateStat.st_mode))
		{
			resolvedPath = candidate;
			return true;
		}
	}
	return false;
}

static HttpResponse buildErrorResponse(int statusCode, const LocationConfig* location)
{
	HttpResponse response;
	if (location != NULL && !location->getErrorPages().empty())
		ErrorPage::tryBuild(statusCode, location->getErrorPages(), response);
	else
		ErrorPage::tryBuildDefault(statusCode, response);
	return response;
}

static bool isRedirectStatus(int statusCode)
{
	return statusCode >= 300 && statusCode < 400;
}

static std::string stripQueryString(const std::string& uri)
{
	std::string::size_type queryPos = uri.find('?');
	if (queryPos == std::string::npos)
		return uri;
	return uri.substr(0, queryPos);
}

static bool hasExtension(const std::string& path, const std::string& extension)
{
	if (extension.empty())
		return false;
	if (path.length() < extension.length())
		return false;
	return path.compare(path.length() - extension.length(), extension.length(), extension) == 0;
}

static bool parseCgiOutput(const std::string& rawOutput, HttpResponse& response)
{
	std::string::size_type separator = rawOutput.find("\r\n\r\n");
	std::string::size_type separatorLength = 4;
	if (separator == std::string::npos)
	{
		separator = rawOutput.find("\n\n");
		separatorLength = 2;
	}

	std::string headerBlock;
	std::string body;
	if (separator == std::string::npos)
	{
		body = rawOutput;
	}
	else
	{
		headerBlock = rawOutput.substr(0, separator);
		body = rawOutput.substr(separator + separatorLength);
	}

	int statusCode = 200;
	bool hasContentType = false;
	std::string::size_type lineStart = 0;
	while (lineStart < headerBlock.size())
	{
		std::string::size_type lineEnd = headerBlock.find("\n", lineStart);
		std::string line;
		if (lineEnd == std::string::npos)
			line = headerBlock.substr(lineStart);
		else
			line = headerBlock.substr(lineStart, lineEnd - lineStart);
		if (!line.empty() && line[line.length() - 1] == '\r')
			line.erase(line.length() - 1);
		if (!line.empty())
		{
			std::string::size_type colon = line.find(':');
			if (colon != std::string::npos)
			{
				std::string key = line.substr(0, colon);
				std::string value = line.substr(colon + 1);
				while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
					value.erase(0, 1);
				if (key == "Status")
				{
					statusCode = std::atoi(value.c_str());
				}
				else if (key == "Content-Type")
				{
					response.setHeader("Content-Type", value);
					hasContentType = true;
				}
				else
				{
					response.setHeader(key, value);
				}
			}
		}
		if (lineEnd == std::string::npos)
			break;
		lineStart = lineEnd + 1;
	}

	response.setStatusCode(statusCode);
	response.setBody(body);
	if (!hasContentType)
		response.setHeader("Content-Type", "text/plain");
	return true;
}

static bool writeAll(int fd, const char* data, size_t size)
{
	size_t totalWritten = 0;
	while (totalWritten < size)
	{
		ssize_t written = write(fd, data + totalWritten, size - totalWritten);
		if (written < 0)
			return false;
		totalWritten += static_cast<size_t>(written);
	}
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
		response = ErrorPage::buildDefault(413);
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

	const std::string requestPath = stripQueryString(request.getPath());

	const LocationConfig* location = matchLocation(requestPath);

	// 1. HAndle 404 Not Found
	if (location == NULL)
	{
		Logger::info("Router: No location found for " + request.getPath());
		response = ErrorPage::buildDefault(404);
		return;
	}

	if (location->hasRedirect())
	{
		int statusCode = location->getRedirectStatusCode();
		if (!isRedirectStatus(statusCode))
			statusCode = 302;
		response.setStatusCode(statusCode);
		response.setHeader("Location", location->getRedirectTarget());
		response.setBody("");
		return;
	}

	// 2. Handle 405 Method Not Allowed
	if (!location->isMethodAllowed(request.getMethod()))
	{
		Logger::warning("Router: 405 Method Not Allowed (" + request.getMethod() + ") for " + requestPath);
		response = buildErrorResponse(405, location);
		response.setHeader("Allow", location->getAllowedMethodsHeader());
		return;
	}

	if (isBodyTooLarge(request, location))
	{
		Logger::warning("Router: request body too large for route " + location->getPath());
		response = buildErrorResponse(413, location);
		return;
	}

	std::string physicalPath = translatePath(requestPath, location);
	if (handleCgi(request, physicalPath, location, response))
		return;

	// 3. Translate the path and route to the correct handler

	if (request.getMethod() == "GET"){
		handleGet(requestPath, physicalPath, location, response);
	} else if (request.getMethod() == "POST") {
		handlePost(request, location, response);
	} else if (request.getMethod() == "DELETE"){
		handleDelete(physicalPath, location, response);
	} else {
		// Handle 501 Not Implemented (for methods like PUT, PATCH if not implemented)
		response = ErrorPage::buildDefault(501);
	}
}

bool Router::handleCgi(const RequestParser& request, const std::string& physicalPath, const LocationConfig* location, HttpResponse& response) const
{
	if (location == NULL)
		return false;
	if (location->getCgiExtension().empty() || location->getCgiExecutable().empty())
		return false;
	if (!hasExtension(physicalPath, location->getCgiExtension()))
		return false;

	struct stat info;
	if (stat(physicalPath.c_str(), &info) != 0 || S_ISDIR(info.st_mode))
	{
		response = buildErrorResponse(404, location);
		return true;
	}

	CgiHandler cgi;
	if (!cgi.init(request, physicalPath, location->getCgiExecutable()))
	{
		response = buildErrorResponse(500, location);
		return true;
	}

	const std::vector<char>& body = request.getBody();
	if (!body.empty())
	{
		if (!writeAll(cgi.getWriteFd(), &body[0], body.size()))
		{
			close(cgi.getWriteFd());
			close(cgi.getReadFd());
			waitpid(cgi.getPid(), NULL, 0);
			response = buildErrorResponse(500, location);
			return true;
		}
	}
	close(cgi.getWriteFd());

	int status = 0;
	waitpid(cgi.getPid(), &status, 0);

	int flags = fcntl(cgi.getReadFd(), F_GETFL, 0);
	if (flags != -1)
		fcntl(cgi.getReadFd(), F_SETFL, flags & ~O_NONBLOCK);

	std::string output;
	char buffer[1024];
	ssize_t bytesRead = 0;
	while ((bytesRead = read(cgi.getReadFd(), buffer, sizeof(buffer))) > 0)
		output.append(buffer, static_cast<size_t>(bytesRead));
	close(cgi.getReadFd());

	if (output.empty())
	{
		response = buildErrorResponse(500, location);
		return true;
	}

	parseCgiOutput(output, response);
	return true;
}

bool Router::isBodyTooLarge(const RequestParser& request, const LocationConfig* location) const
{
	if (!location)
		return false;
	if (location->getClientMaxBodySize() == 0)
		return false;
	return request.getBody().size() > location->getClientMaxBodySize();
}

void Router::handleDelete(const std::string& physicalPath, const LocationConfig* location, HttpResponse& response) const
{
	struct stat buffer;
	// check if the file exists using stat
	if (stat(physicalPath.c_str(), &buffer) != 0)
	{
		Logger::info("DELETE Error: File not found - " + physicalPath);
		response = buildErrorResponse(404, location);
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
		response = buildErrorResponse(403, location);
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

	if (stat(targetPath.c_str(), &pathStat) != 0)
	{
		Logger::warning("GET eRROR: File not found - " + targetPath);
		response = buildErrorResponse(404, location);
		return;
	}

	requestIsDirectory = S_ISDIR(pathStat.st_mode);
	if (requestIsDirectory)
	{
		if (resolveIndexFile(targetPath, location, indexPath))
		{
			targetPath = indexPath;
			if (stat(targetPath.c_str(), &indexStat) == 0)
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
			response = buildErrorResponse(403, location);
			return;
		}
	}

	if (stat(targetPath.c_str(), &pathStat) != 0 || S_ISDIR(pathStat.st_mode))
	{
		Logger::warning("GET Error: Index file not found in directory - " + targetPath);
		response = buildErrorResponse(403, location);
		return;
	}
	// check permission for read
	if (!(pathStat.st_mode & S_IRUSR))
	{
		Logger::warning("GET Error: Permission denied - " + targetPath);
		response = buildErrorResponse(403, location);
		return;
	}

	std::ifstream file(targetPath.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open())
	{
		Logger::error("GET Error: Could not open file - " + targetPath);
		response = buildErrorResponse(500, location);
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
	if (isBodyTooLarge(request, location))
	{
		response = buildErrorResponse(413, location);
		return;
	}

	if (location != NULL && location->isUploadEnabled())
	{
		const std::map<std::string, std::string>& headers = request.getHeaders();
		std::map<std::string, std::string>::const_iterator contentType = headers.find("content-type");
		std::string boundary;
		std::string filename;
		std::string fileBody;
		const std::vector<char>& rawBody = request.getBody();
		const std::string body(rawBody.begin(), rawBody.end());

		if (contentType == headers.end() || !extractBoundary(contentType->second, boundary)
			|| !parseMultipartFile(body, boundary, filename, fileBody))
		{
			response = buildErrorResponse(400, location);
			return;
		}
		UploadHandler::handleUpload(location->getUploadStore(), filename, fileBody, 0, response);
		return;
	}

	std::string cgiExecutable = "";
	std::string scriptPath = request.getPath();

	if (location != NULL)
	{
		cgiExecutable = location->getPath();
		if (!location->getRoot().empty())
		{
			scriptPath = location->getRoot() + request.getPath();
		}
	}

	if (cgiExecutable.empty())
	{
		Logger::warning("POST request rejected: No CGI executable configured for this location.");
		response = ErrorPage::buildDefault(403);
		return;
	}

	Logger::info("CGI execution triggered for: " + scriptPath + " using executable: " + cgiExecutable);

	CgiHandler cgi;
	if (cgi.init(request, scriptPath, cgiExecutable))
	{
		response.setCgi(cgi.getReadFd(), cgi.getPid());
	}
	else
	{
		Logger::error("Failed to initialize CGI procerss.");
		response = ErrorPage::buildDefault(500);
	}

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
