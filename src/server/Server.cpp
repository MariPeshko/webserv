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

Server::~Server() {
}