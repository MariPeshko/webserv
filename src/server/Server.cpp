#include "Server.hpp"


// for now set up mock data
Server::Server() {
    _port = 8080;
    _host = "127.0.0.1";
    _server_names.push_back("localhost");
    _root = "/var/www/html";
    _index = "index.html";
    _error_pages[404] = "/var/www/errors/404.html";
	_client_max_body_size = "1m"; // default value
}

// select() examines the file descriptor sets reading_set and writing_set to see if any of their descriptors are ready for I/O.

int Server::setupServer() {
    // setup server
    
    // create socket
    // fd = socket(AF_INET, SOCK_STREAM, 0);

    // bind socket
    // bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // listen for connections

    // accept connections
    //client_fd = accept(server_fd, NULL, NULL);

    // read requests
    // send responses
    // close connections
    // close socket
    // return success
    return 0;
}

Server::~Server() {
}