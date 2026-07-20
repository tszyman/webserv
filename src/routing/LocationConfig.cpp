#include "routing/LocationConfig.hpp"
#include <algorithm>

LocationConfig::LocationConfig(const std::string& path, const std::string& root) : 
_path(path), _root(root), _autoindex(false), _uploadEnabled(false), _uploadStore(""),
_index("index.html"), _maxBodySize(0), _hasMaxBodySize(false), _cgiExtension(""), _cgiPath("") {}

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

void LocationConfig::setIndex(const std::string& index) { _index = index; }
void LocationConfig::setMaxBodySize(size_t maxBodySize) { _maxBodySize = maxBodySize; _hasMaxBodySize = true; }
void LocationConfig::setCgi(const std::string& extension, const std::string& executable)
{
	_cgiExtension = extension;
	_cgiPath = executable;
}
const std::string& LocationConfig::getIndex() const { return _index; }
bool LocationConfig::hasMaxBodySize() const { return _hasMaxBodySize; }
size_t LocationConfig::getMaxBodySize() const { return _maxBodySize; }
bool LocationConfig::isCgiEnabled() const { return !_cgiExtension.empty() && !_cgiPath.empty(); }
const std::string& LocationConfig::getCgiExtension() const { return _cgiExtension; }
const std::string& LocationConfig::getCgiPath() const { return _cgiPath; }
