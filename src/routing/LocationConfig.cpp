#include "routing/LocationConfig.hpp"
#include <algorithm>

LocationConfig::LocationConfig(const std::string& path, const std::string& root) : 
_path(path), _root(root), _allowedMethods(), _indexFiles(), _hasRedirect(false), _redirectStatusCode(301), _redirectTarget(""), _errorPages(), _autoindex(false), _uploadEnabled(false), _uploadStore(""), _clientMaxBodySize(0) {}

void LocationConfig::addAllowedMethod(const std::string& method)
{
	if(std::find(_allowedMethods.begin(), _allowedMethods.end(), method) == _allowedMethods.end())
		_allowedMethods.push_back(method);
}

void LocationConfig::setIndexFiles(const std::vector<std::string>& indexFiles)
{
	_indexFiles = indexFiles;
}

void LocationConfig::setRedirect(int statusCode, const std::string& target)
{
	_hasRedirect = true;
	_redirectStatusCode = statusCode;
	_redirectTarget = target;
}

void LocationConfig::addErrorPage(int statusCode, const std::string& filePath)
{
	_errorPages[statusCode] = filePath;
}

void LocationConfig::setAutoindex(bool enabled)
{
	_autoindex = enabled;
}

void LocationConfig::setUpload(bool enabled, const std::string& storePath)
{
	_uploadEnabled = enabled;
	_uploadStore = storePath;
}

void LocationConfig::setClientMaxBodySize(size_t maxBodySize)
{
	_clientMaxBodySize = maxBodySize;
}

bool LocationConfig::isMethodAllowed(const std::string& method) const
{
	if(_allowedMethods.empty())
		return false;
	return (std::find(_allowedMethods.begin(), _allowedMethods.end(), method) != _allowedMethods.end());
}

const std::string& LocationConfig::getPath() const
{
	return _path;
}

const std::string& LocationConfig::getRoot() const
{
	return _root;
}

const std::vector<std::string>& LocationConfig::getIndexFiles() const
{
	return _indexFiles;
}

bool LocationConfig::hasRedirect() const
{
	return _hasRedirect;
}

int LocationConfig::getRedirectStatusCode() const
{
	return _redirectStatusCode;
}

const std::string& LocationConfig::getRedirectTarget() const
{
	return _redirectTarget;
}

const std::map<int, std::string>& LocationConfig::getErrorPages() const
{
	return _errorPages;
}

bool LocationConfig::getAutoindex() const
{
	return _autoindex;
}
bool LocationConfig::isUploadEnabled() const
{
	return _uploadEnabled;
}
const std::string& LocationConfig::getUploadStore() const
{
	return _uploadStore;
}

size_t LocationConfig::getClientMaxBodySize() const
{
	return _clientMaxBodySize;
}