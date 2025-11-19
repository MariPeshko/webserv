#include "Request.hpp"

Request::Request() : _parsing_state(REQUEST_LINE) {
}

Request::~Request() {
}

void Request::setMethod(const std::string &method) {
    if (method == "GET") {
        _method = GET;
    } else if (method == "POST") {
        _method = POST;
    } else if (method == "DELETE") {
        _method = DELETE;
    }
}

void Request::setUri(const std::string &uri) {
    _uri = uri;
}

void Request::setVersion(const std::string &version) {
    _version = version;
}

void Request::addHeader(const std::string &key, const std::string &value) {
    _headers[key] = value;
}

void Request::setBody(const std::string &body) {
    _body = body;
}

void Request::setParsingState(ParsingState state) {
    _parsing_state = state;
}

std::string Request::getMethod() const {
    switch (_method) {
        case GET: return "GET";
        case POST: return "POST";
        case DELETE: return "DELETE";
        default: return "";
    }
}

std::string Request::getUri() const {
    return _uri;
}

std::string Request::getVersion() const {
    return _version;
}

std::map<std::string, std::string> Request::getHeaders() const {
    return _headers;
}

std::string Request::getBody() const {
    return _body;
}

void Request::parseRequest(char* raw_request, size_t length) {
    // Implementation for parsing raw_request and populating Request fields
}