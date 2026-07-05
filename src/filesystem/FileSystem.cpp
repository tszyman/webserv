#include "filesystem/FileSystem.hpp"

std::string FileSystem::getExtension(const std::string& path)
{
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string::npos)
    {
        return "";
    }
    return path.substr(dot_pos);
}

std::string FileSystem::readFile(const std::string& path)
{
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("File not found or permission denied: " + path);
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string FileSystem::getMimeType(const std::string& path)
{
    std::string ext = getExtension(path);

    if (ext == ".html" || ext == ".htm")    return "text/html";
    if (ext == ".css")                      return "text/css";
    if (ext == ".js")                       return "application/javascript";
    if (ext == ".png")                      return "image/png";
    if (ext == ".jpg" || ext == ".jpeg")    return "image/jpeg";
    if (ext == ".gif")                      return "imager/gif";
    if (ext == ".ico")                      return "image/x-icon";
    if (ext == ".txt")                      return "text/plain";
    if (ext == ".json")                     return "application/json";
    
    return "application/octet-stream"; //default extension for unrecognized file type
}

bool FileSystem::fileExists(const std::string& path)
{
    std::ifstream file(path.c_str());
    return file.is_open();
}