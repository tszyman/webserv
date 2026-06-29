#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>

class LocationConfig {
	public:
		LocationConfig(const std::string& path, const std::string& root);

		void addAllowedMethod(const std::string& method);

		// Setters for optional config directives
		void setAutoindex(bool enabled);
		void setUpload(bool enabled, const std::string& storePath = "");

		bool isMethodAllowed(const std::string& method) const;
		const std::string& getPath() const;
		const std::string& getRoot() const;
		bool getAutoindex() const;
		bool isUploadEnabled() const;
		const std::string& getUploadStore() const;

	private:
		std::string _path; // e.g. "/images/"
		std::string _root; // e.g. "/var/www/data"
		std::vector<std::string> _allowedMethods; // e.g. ["GET", "DELETE"]

		bool _autoindex;
		bool _uploadEnabled;
		std::string _uploadStore;
};

#endif // LOCATIONCONFIG_HPP