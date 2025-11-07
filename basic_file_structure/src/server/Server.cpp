#include "Server.hpp"

// for now set up mock data
Server::Server() {
    _port = 8080;
    _host = "127.0.0.1";
    _name = "localhost";
    _root = "/var/www/html";
    _index = "index.html";
    _error_page = "/var/www/errors/404.html";
    _cgi = "/usr/bin/php-cgi";
    _location = "/";
}

Server::Server(int port, std::string host, std::string name, std::string root, std::string index, std::string error_page, std::string cgi, std::string location) {
    _port = port;
    _host = host;
    _name = name;
    _root = root;
    _index = index;
    _error_page = error_page;
    _cgi = cgi;
    _location = location;
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