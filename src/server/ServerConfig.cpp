#include "ServerConfig.hpp"

ServerConfig::ServerConfig() {
}

ServerConfig::~ServerConfig() {
}

void ServerConfig::setupServer() {
    // create socket
    _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen_fd == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }
    // bind socket
    bind(_listen_fd, (struct sockaddr *)&_server_address, sizeof(_server_address));
    if (_listen_fd == -1) {
        std::cerr << "Failed to bind socket" << std::endl;
        return;
    }
    // listen for connections
    if (listen(_listen_fd, 10) == -1) {
        std::cerr << "Failed to listen on socket" << std::endl;
        return;
    }
    std::cout << "Server is listening on port " << _port << std::endl;
}