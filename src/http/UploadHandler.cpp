#include "../../include/http/UploadHandler.hpp"
#include "../../include/http/HttpErrorPage.hpp"
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>

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

std::string UploadHandler::joinPath(const std::string &directory, const std::string &filename)
{
    if (directory.empty() || directory == "")
        return "/" + filename;
    if (directory[directory.size() - 1] == '/')
        return directory + filename;
    return directory + "/" + filename;
}