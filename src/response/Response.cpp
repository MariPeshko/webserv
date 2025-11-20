#include "Response.hpp"

Response::Response() {
}

Response::~Response() {
}

void Response::setRequest( Request& req) {
    request = req;
}

void Response::setServerConfig(Server& server) {
    _server_config = server;
}

void Response::generateResponse() {
    _statusCode = 200;

    const std::string path = "www/index.html";
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        _statusCode = 404;
        _responseBody = "Not Found";
        std::cout << "Failed to open file: " << path << std::endl;
        return;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    _responseBody = ss.str();
    file.close();
    _contentLength = _responseBody.size();
}

short Response::getStatusCode() {
    return _statusCode;
}

size_t Response::getContentLength() {
    return _contentLength;
}

std::string Response::getResponseBody() {
    return _responseBody;
}
