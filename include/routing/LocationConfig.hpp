#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <map>
#include <string>
#include <vector>

class LocationConfig {
	public:
		LocationConfig(const std::string& path, const std::string& root);

		void addAllowedMethod(const std::string& method);
		void setIndexFiles(const std::vector<std::string>& indexFiles);
		void setRedirect(int statusCode, const std::string& target);
		void addErrorPage(int statusCode, const std::string& filePath);
		void setCgi(const std::string& extension, const std::string& executable);

		// Setters for optional config directives
		void setAutoindex(bool enabled);
		void setUpload(bool enabled, const std::string& storePath = "");
		void setClientMaxBodySize(size_t maxBodySize);

		bool isMethodAllowed(const std::string& method) const;
		std::string getAllowedMethodsHeader() const;
		const std::string& getPath() const;
		const std::string& getRoot() const;
		const std::vector<std::string>& getIndexFiles() const;
		bool hasRedirect() const;
		int getRedirectStatusCode() const;
		const std::string& getRedirectTarget() const;
		const std::map<int, std::string>& getErrorPages() const;
		const std::string& getCgiExtension() const;
		const std::string& getCgiExecutable() const;
		bool getAutoindex() const;
		bool isUploadEnabled() const;
		const std::string& getUploadStore() const;
		size_t getClientMaxBodySize() const;

	private:
		std::string _path; // e.g. "/images/"
		std::string _root; // e.g. "/var/www/data"
		std::vector<std::string> _allowedMethods; // e.g. ["GET", "DELETE"]
		std::vector<std::string> _indexFiles;
		bool _hasRedirect;
		int _redirectStatusCode;
		std::string _redirectTarget;
		std::map<int, std::string> _errorPages;
		std::string _cgiExtension;
		std::string _cgiExecutable;

		bool _autoindex;
		bool _uploadEnabled;
		std::string _uploadStore;
		size_t _clientMaxBodySize;
};

#endif // LOCATIONCONFIG_HPP
