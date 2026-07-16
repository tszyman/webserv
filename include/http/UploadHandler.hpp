#ifndef UPLOAD_HANDLER_HPP
# define UPLOAD_HANDLER_HPP

# include "http/HttpResponse.hpp"
# include <string>

class UploadHandler
{
    public:
        enum TargetStatus
        {
                TARGET_OK,
                TARGET_EMPTY_DIRECTORY,
                TARGET_DIRECTORY_NOT_FOUND,
                TARGET_NOT_DIRECTORY,
                TARGET_DIRECTORY_FORBIDDEN,
                TARGET_EMPTY_FILENAME,
                TARGET_UNSAFE_FILENAME
        };

        static TargetStatus validateTarget(const std::string &uploadDirectory, const std::string &filename, std::string &targetPath);
        static bool prepareTarget(const std::string &uploadDirectory, const std::string &filename, std::string &targetPath, HttpResponse &errorResponse);
        static bool handleUpload(const std::string &uploadDirectory, const std::string &filename, const std::string &body, unsigned long maxBodySize, HttpResponse &response);
        static int httpSttusFor(TargetStatus status);
        static const char *messageFor(TargetStatus status);

    private:
        UploadHandler();
        UploadHandler(const UploadHandler &other);
        UploadHandler &operator=(const UploadHandler &other);
        ~UploadHandler();

        static bool isSafeFilename(const std::string &filename);
        static TargetStatus validateUploadDirectory(const std::string &path);
        static bool hasWritePermission(const std::string &path);
        static bool writeBodyToFile(const std::string &targetPath, const std::string &body);
        static HttpResponse buildCreatedResponse();
        static std::string joinPath(const std::string &directory, const std::string &filename);
    };

#endif