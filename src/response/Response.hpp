#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>

class Response {
    public:
        Response();
        ~Response();

    private:
        // short   _statusCode;
        std::string _responseBody;
        // size_t  _contentLength;
};

#endif // RESPONSE_HPP