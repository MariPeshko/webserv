#include "Client.hpp"

Client::Client() {
}

Client::~Client() {
}

Client::Client(const Client &other) {
    _fd = other._fd;
    _client_address = other._client_address;
}

void Client::setFd(int fd) {
    _fd = fd;
}

void Client::setClientAddress(const sockaddr_in& client_address) {
    _client_address = client_address;
}

std::string Client::getResponseString() {
    std::string body = response.getResponseBody();
    short status_code = response.getStatusCode();
    size_t content_length = response.getContentLength();
    
    std::ostringstream oss;

    // 1. Status Line
    oss << "HTTP/1.1 " << status_code << " OK\r\n";

    // 2. Headers
    oss << "Content-Type: text/html\r\n";
    oss << "Content-Length: " << content_length << "\r\n";
    oss << "Connection: keep-alive\r\n"; // Optional

    // 3. Empty Line (End of headers)
    oss << "\r\n";
    oss << body;

    return oss.str();
}

int Client::getFd() {
    return _fd;
}