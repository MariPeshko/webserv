#include "Server.hpp"
#include "../logger/Logger.hpp"
#include <arpa/inet.h> // For inet_addr
#include <unistd.h>	   // For close
#include <string.h>	   // For memset
#include <cerrno>	   // errno
#include <sstream>	   // For stringstream

// for now set up mock data
Server::Server()
{
	_host = "127.0.0.1";
	_client_max_body_size = "1m"; // default value
	_listen_fd = -1;			  // an invalid value
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

int Server::setupServer() {
	// create socket
	_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listen_fd == -1)
	{
		Logger::logErrno(LOG_ERROR, "Failed to create socket");
		return -1;
	}

	int lflags = fcntl(_listen_fd, F_GETFL, 0);
	if (lflags != -1)
		fcntl(_listen_fd, F_SETFL, lflags | O_NONBLOCK);

	int yes = 1;
	//TODO: REPLACE SO_REUSEADDR WITH SO_KEEPALIVE
	if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
	{
		Logger::logErrno(LOG_ERROR, "Failed to set SO_REUSEADDR");
	}

	struct sockaddr_in server_address = {};
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(_host.c_str());
	server_address.sin_port = htons(_port);
	_server_address = server_address; // Copy the initialized struct

	if (bind(_listen_fd, (struct sockaddr *)&_server_address, sizeof(_server_address)) == -1)
	{
		std::stringstream ss;
		ss << _port;
		Logger::logErrno(LOG_ERROR, "Failed to bind socket to " + _host + ":" + ss.str());
		close(_listen_fd);
		_listen_fd = -1;
		return -1;
	}

	// listen for connections
	if (listen(_listen_fd, 10) == -1)
	{
		Logger::logErrno(LOG_ERROR, "Failed to listen on socket");
		return -1;
	}
	Logger::log(LOG_INFO, "Server listening on " + _host + ":" + std::to_string(_port));
	return 0;
}

Server::Server(const Server &other)
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
{
}

Server::~Server()
{
	if (_listen_fd != -1)
	{
		close(_listen_fd);
	}
}
