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