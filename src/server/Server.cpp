#include "Server.hpp"
#include <arpa/inet.h>	// For inet_addr
#include <unistd.h>		// For close
#include <string.h>		// For memset
#include <cerrno>		// errno

// for now set up mock data
Server::Server() {
    _port = 8080;
    _host = "127.0.0.1";
    _server_names.push_back("localhost");
    _root = "/var/www/html";
    _index = "index.html";
    _error_pages[404] = "/var/www/errors/404.html";
	_client_max_body_size = "1m"; // default value
    _listen_fd = -1; // an invalid value
}

int	Server::setupServer() {
    // create socket
    _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen_fd == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return -1;
    }

    struct sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(_host.c_str());
    server_address.sin_port = htons(_port);
    _server_address = server_address; // Copy the initialized struct

     if (bind(_listen_fd, (struct sockaddr *)&_server_address, sizeof(_server_address)) == -1) {
        std::cerr << "Failed to bind socket to " << _host << ":" << _port
            << " Error: " << strerror(errno)  << std::endl;
        close(_listen_fd);
        _listen_fd = -1;
        return -1;
    }

    // listen for connections
     if (listen(_listen_fd, 10) == -1) {
        std::cerr << "Failed to listen on socket: " << strerror(errno) << std::endl;
        return -1;
    }
    std::cout << "Server is listening on port " << _port << std::endl;
	
	return 0;
}

Server::~Server() {
    if (_listen_fd != -1) {
        close(_listen_fd);
    }
}
