#ifndef STRINGUTILS_HPP
#define STRINGUTILS_HPP

#include <string>
#include <sstream>

namespace StringUtils
{
    template <typename T>
    std::string to_string(T value)
    {
        std::stringstream ss;
        ss << value;
        return ss.str();
    }
}
#endif