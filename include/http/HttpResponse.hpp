#ifndef HTTP_RESPONSE_HPP
# define HTTP_RESPONSE_HPP

# include <map>
# include <string>
# include <sys/types.h>

class HttpResponse
{
    public:
        typedef std::map<std::string, std::string> HeaderMap;

        HttpResponse();
        HttpResponse(int statusCode, const std::string &body);
        ~HttpResponse();

        int getStatusCode() const;
        const std::string &getBody() const;
        const HeaderMap &getHeaders() const;

        void setStatusCode(int statusCode);
        void setBody(const std::string &body);
        void setHeader(const std::string &name, const std::string &value);
        void setConnectionClose(bool closeConnection);
        void serveStaticFile(const std::string& filePath);

        std::string toString() const;
        
        static std::string numberToString(unsigned long value);

        void setCgi(int readFd, pid_t pid);
        bool isCgi() const;
        int getCgiReadFd() const;
        pid_t getCgiPid() const;
        private:
            int _statusCode;
            HeaderMap _headers;
            std::string _body;
            bool _closeConnection;
            bool _is_cgi;
            int _cgi_read_fd;
            pid_t _cgi_pid;
};

#endif