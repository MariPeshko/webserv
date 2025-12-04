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
	_root = "www/web";
	_index = "index.html";
	_error_pages[404] = "/var/www/errors/404.html";
	_client_max_body_size = "1m"; // default value
	_listen_fd = -1; // an invalid value
}

void	Location::printLocation() const {
	std::cout << "    Location path: " << _path << std::endl;
	if (!_root.empty()) std::cout << "      root: " << _root << std::endl;
	if (!_alias.empty()) std::cout << "      alias: " << _alias << std::endl;
	if (!_allowed_methods.empty()) {
		std::cout << "      methods: ";
		for (size_t i = 0; i < _allowed_methods.size(); i++) {
			std::cout << _allowed_methods[i] << (i < _allowed_methods.size() - 1 ? ", " : "");
		}
		std::cout << std::endl;
	}
	if (!_index.empty()) std::cout << "      index: " << _index << std::endl;
	if (!_client_max_body_size.empty()) std::cout << "      client_max_body_size: " << _client_max_body_size << std::endl;
	std::cout << "      autoindex: " << (_autoindex ? "on" : "off") << std::endl;
	if (_return_code != 0) {
		std::cout << "      return: " << _return_code << " " << _return_url << std::endl;
	}
	if (!_cgi.empty()) {
		std::cout << "      cgi:" << std::endl;
		for (std::map<std::string, std::string>::const_iterator it = _cgi.begin(); it != _cgi.end(); ++it) {
			std::cout << "        " << it->first << " -> " << it->second << std::endl;
		}
	}
	if (!_locations.empty()) {
		std::cout << "      Nested Locations:" << std::endl;
		for (size_t i = 0; i < _locations.size(); ++i) {
			_locations[i].printLocation();
		}
	}
}

int	Server::setupServer() {
	// create socket
	_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listen_fd == -1) {
		std::cerr << "Failed to create socket" << std::endl;
		return -1;
	}

	int	lflags = fcntl(_listen_fd, F_GETFL, 0);
	if (lflags != -1)
		fcntl(_listen_fd, F_SETFL, lflags | O_NONBLOCK);

	int	yes = 1;
	if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		std::cerr << "Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
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
	std::cout << "IP(host): " << this->_host << std::endl;

	return 0;
}

Server::Server(const Server& other)
			: _port(other._port),
			  _host(other._host),
			  _server_names(other._server_names),
			  _root(other._root),
			  _index(other._index),
			  _error_pages(other._error_pages),
			  _locations(other._locations),
			  _client_max_body_size(other._client_max_body_size),
			  _server_address(other._server_address),
			  _listen_fd(other._listen_fd)
{  }

Server::~Server() {
	if (_listen_fd != -1) {
		close(_listen_fd);
	}
}
