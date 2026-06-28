#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>

class LocationConfig {
	public:
		LocationConfig(const std::string& path, const std::string& root);

		void addAllowedMethod(const std::string& method);

		bool isMethodAllowed(const std::string& method) const;
		const std::string& getPath() const;
		const std::string& getRoot() const;
			
	private:
		std::string _path; // e.g. "/images/"
		std::string _root; // e.g. "/var/www/data"
		std::vector<std::string> _allowedMethods; // e.g. ["GET", "DELETE"]
};

#endif // LOCATIONCONFIG_HPP