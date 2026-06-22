#include "routing/LocationConfig.hpp"
#include <algorithm>

LocationConfig::LocationConfig(const std::string& path, const std::string& root) : _path(path), _root(root) {}

void LocationConfig::addAllowedMethod(const std::string& method)
{
	_allowedMethods.push_back(method);
}

bool LocationConfig::isMethodAllowed(const std::string& method) const
{
	// check if the requested method exists in our vector of allowed methods
	return std::find(_allowedMethods.begin(), _allowedMethods.end(), method) != _allowedMethods.end();
}

const std::string& LocationConfig::getPath() const
{
	return _path;
}

const std::string& LocationConfig::getRoot() const
{
	return _root;
}
