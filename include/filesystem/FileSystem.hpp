#ifndef FILESYSTEM_HPP
#define FILESYSTEM_HPP

#include <string>
#include <stdexcept>
#include <fstream>
#include <sstream>

class FileSystem
{
    private:
        FileSystem();
        ~FileSystem();
        FileSystem(const FileSystem&);
        FileSystem& operator=(const FileSystem&);

        static std::string getExtension(const std::string& path);

    public:
        static std::string readFile(const std::string& path);
        static std::string getMimeType(const std::string& path);
        static bool fileExists(const std::string &path);
};

#endif