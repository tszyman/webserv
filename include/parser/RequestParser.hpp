class RequestParser
{
    public:
        RequestParser();
        void feed(const std::string &data);
        bool isComplete() const;
};