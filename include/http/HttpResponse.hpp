#ifndef HTTP_RESPONSE_HPP
# define HTTP_RESPONSE_HPP

# include <map>
# include <string>

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

        std::string toString() const;
        
        static std::string numberToString(unsigned long value);

        private:
            int _statusCode;
            HeaderMap _headers;
            std::string _body;
            bool _closeConnection;
};

#endif