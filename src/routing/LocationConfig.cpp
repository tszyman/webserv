#include "routing/LocationConfig.hpp"
#include <algorithm>

LocationConfig::LocationConfig(const std::string& path, const std::string& root) : 
_path(path), _root(root), _allowedMethods(), _autoindex(false), _uploadEnabled(false), _uploadStore(""), _clientMaxBodySize(0) {}

void LocationConfig::addAllowedMethod(const std::string& method)
{
	if(std::find(_allowedMethods.begin(), _allowedMethods.end(), method) == _allowedMethods.end())
		_allowedMethods.push_back(method);
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

std::string LocationConfig::getAllowedMethodsHeader() const
{
	std::string methods;
	for (size_t i = 0; i < _allowedMethods.size(); ++i)
	{
		if (i != 0)
			methods += ", ";
		methods += _allowedMethods[i];
	}
	return methods;
}

const std::string& LocationConfig::getPath() const
{
	return _path;
}

const std::string& LocationConfig::getRoot() const
{
	return _root;
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
