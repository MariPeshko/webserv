#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

class Response {
    public:
        Response();
        ~Response();

        void   generateResponse(); 
        std::string getResponseBody();
        std::string getStatusCode();
        size_t      getContentLength();

    private:
        short   _statusCode;
        size_t  _contentLength;
        std::string _responseBody;
};

#endif // RESPONSE_HPP