#include "Response.hpp"

// Parametric constructor
Response::Response(Server& server)
    : _server_config(server),
      _request(0),
      _statusCode(200),
      _reasonPhrase("OK"),
      _contentLength(0)
{ }

Response&	Response::operator= (const Response & other) {
	if (this != &other) {
		_server_config = other._server_config;
		_request = other._request;
		_statusCode = other.getStatusCode();
		_reasonPhrase = other._reasonPhrase;
		_contentLength = other.getContentLength();
	}
	return *this;
}

Response::~Response() { }

// call after parsing
void Response::bindRequest(const Request& req) {
    _request = &req;
}

void Response::generatePath() {
	std::cout << " test: " << _server_config.getLocations().begin()->_path << std::endl;

}

void Response::generateResponse() {
    _statusCode = 200;
	generatePath();

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

short Response::getStatusCode() const { return _statusCode; }

size_t Response::getContentLength() const { return _contentLength; }

std::string Response::getResponseBody() const {
    return _responseBody;
}

/** nulls the Request* before new HTTP request-response cycle */
void Response::reset() { // Maryna's suggestion. Ivan: "Я думаю обнулити поінтер достатньо"
    _request = 0;
}
