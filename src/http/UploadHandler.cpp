#include "../../include/http/UploadHandler.hpp"
#include "../../include/http/HttpErrorPage.hpp"
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace
{
    class FdGuard
    {
        public:
            explicit FdGuard(int fd) : _fd(fd) {}
            ~FdGuard()
            {
                if (_fd >= 0)
                    close(_fd);
            }
        
            private:
                FdGuard(const FdGuard &other);
                FdGuard &operator=(const FdGuard &other);
                int _fd;
    };
}

UploadHandler::TargetStatus UploadHandler::validateTarget(const std::string &uploadDirectory, const std::string &filename, std::string &targetPath)
{
    TargetStatus directoryStatus;

    targetPath.clear();
    if (!isSafeFilename(filename))
    {
        if (filename.empty())
            return TARGET_EMPTY_FILENAME;
        return TARGET_UNSAFE_FILENAME;
    }
    directoryStatus = validateUploadDirectory(uploadDirectory);
    if (directoryStatus != TARGET_OK)
        return directoryStatus;
    targetPath = joinPath(uploadDirectory, filename);
    return TARGET_OK;
}

bool UploadHandler::prepareTarget(const std::string &uploadDirectory, const std::string &filename, std::string &targetPath, HttpResponse &errorResponse)
{
    const TargetStatus status = validateTarget(uploadDirectory, filename, targetPath);
    if (status == TARGET_OK)
        return true;
    return ErrorPage::tryBuildDefault(httpSttusFor(status), errorResponse);
}

bool UploadHandler::handleUpload(const std::string &uploadDirectory, const std::string &filename, const std::string &body, unsigned long maxBodySize, HttpResponse &response)
{
    std::string targetPath;

    if (!prepareTarget(uploadDirectory, filename, targetPath, response))
        return false;
    if (body.size() > maxBodySize)
    {
        ErrorPage::tryBuildDefault(413, response);
        return false;
    }
    if (!writeBodyToFile(targetPath, body))
    {
        ErrorPage::tryBuildDefault(500, response);
        return false;
    }
    response = buildCreatedResponse();
    return true;
}

int UploadHandler::httpSttusFor(TargetStatus status)
{
    if (status == TARGET_EMPTY_FILENAME || status == TARGET_UNSAFE_FILENAME)
        return 400;
    if (status == TARGET_DIRECTORY_FORBIDDEN)
        return 403;
    if (status == TARGET_EMPTY_DIRECTORY || status == TARGET_DIRECTORY_NOT_FOUND || status == TARGET_NOT_DIRECTORY)
        return 500;
    return 500;
}

const char *UploadHandler::messageFor(TargetStatus status)
{
    switch (status)
    {
        case TARGET_OK: return "upload target is usable";
        case TARGET_EMPTY_DIRECTORY: return "upload directory is empty";
        case TARGET_DIRECTORY_NOT_FOUND: return "upload directory does not exist";
        case TARGET_NOT_DIRECTORY: return "upload path is not a directory";
        case TARGET_DIRECTORY_FORBIDDEN: return "upload directory is not writable";
        case TARGET_EMPTY_FILENAME: return "upload filename is empty";
        case TARGET_UNSAFE_FILENAME: return "upload filename is unsafe";
        default: return "unknown upload target status";
    }
}

bool UploadHandler::isSafeFilename(const std::string &filename)
{
    std::string::const_iterator it;

    if (filename.empty() || filename == "." || filename == "..")
        return false;
    for (it = filename.begin(); it != filename.end(); ++it)
    {
        const unsigned char c = static_cast<unsigned char>(*it);
        if (*it == '/' || *it == '\\')
            return false;
        if (std::iscntrl(c))
            return false;
    }
    return true;
}

UploadHandler::TargetStatus UploadHandler::validateUploadDirectory(const std::string &path)
{
    struct stat info;

    if (path.empty())
        return TARGET_EMPTY_DIRECTORY;
    if (stat(path.c_str(), &info) != 0)
        return TARGET_DIRECTORY_NOT_FOUND;
    if (!S_ISDIR(info.st_mode))
        return TARGET_NOT_DIRECTORY;
    if (!hasWritePermission(path))
        return TARGET_DIRECTORY_FORBIDDEN;
    return TARGET_OK;
}

bool UploadHandler::hasWritePermission(const std::string &path)
{
    return access(path.c_str(), W_OK | X_OK) == 0;
}

bool UploadHandler::writeBodyToFile(const std::string &targetPath, const std::string &body)
{
    int fd;
    std::string::size_type totalWritten;

    fd = open(targetPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return false;
    FdGuard guard(fd);
    totalWritten = 0;
    while (totalWritten < body.size())
    {
        const ssize_t written = write(fd, body.data() + totalWritten, body.size() - totalWritten);
        if (written <= 0)
            return false;
        totalWritten += static_cast<std::string::size_type>(written);
    }
    return true;
}

HttpResponse UploadHandler::buildCreatedResponse()
{
    const std::string body = "Created\n";
    HttpResponse response(201, body);
    response.setHeader("Content-Type", "text/plain");
    response.setHeader("Content-Length", HttpResponse::numberToString(body.size()));
    return response;
}

std::string UploadHandler::joinPath(const std::string &directory, const std::string &filename)
{
    if (directory.empty() || directory == "")
        return "/" + filename;
    if (directory[directory.size() - 1] == '/')
        return directory + filename;
    return directory + "/" + filename;
}