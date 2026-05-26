#ifndef STATUS_CODES_HPP
# define STATUS_CODES_HPP

class HttpStatus
{
    public:
        static const char *reasonPhrase(int statusCode);
        static bool isKnown(int statusCode);
        static bool isError(int statusCode);
    
    private:
        HttpStatus();
        HttpStatus(const HttpStatus &other);
        HttpStatus &operator=(const HttpStatus &other);
        ~HttpStatus();
};

#endif