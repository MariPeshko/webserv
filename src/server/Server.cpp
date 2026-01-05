#include "Server.hpp"
#include "../logger/Logger.hpp"
#include <arpa/inet.h> // For inet_addr
#include <unistd.h>	   // For close
#include <string.h>	   // For memset
#include <cerrno>	   // errno
#include <sstream>	   // For stringstream

// --- Location Implementation ---

Location::Location() : _autoindex(false), _return_code(0) {}
Location::~Location() {}

void	Location::setPath(const std::string& path) {
	_path = path;
}
void	Location::setRoot(const std::string& root) {
	_root = root;
}
void	Location::setIndex(const std::string& index) {
	_index = index;
}
void	Location::setAutoindex(bool autoindex) {
	_autoindex = autoindex;
}
void	Location::addAllowedMethod(const std::string& method) {
	_allowed_methods.push_back(method);
}
void	Location::setReturn(int code, const std::string& url) {
	_return_code = code; _return_url = url;
}
void	Location::addCgi(const std::string& ext, const std::string& path) {
	_cgi[ext] = path;
}
void	Location::addLocation(const Location& location) {
	_locations.push_back(location);
}
void	Location::setAlias(const std::string& alias) {
	_alias = alias;
}
void	Location::setClientMaxBodySize(const std::string& size) {
	_client_max_body_size = size;
}

const std::string&	Location::getPath() const { return _path; }
const std::string&	Location::getRoot() const { return _root; }
const std::string&	Location::getAlias() const { return _alias; }
const std::vector<std::string>&	Location::getAllowedMethods() const {
	return _allowed_methods;
}
const std::string&	Location::getIndex() const {
	return _index;
}
bool				Location::getAutoindex() const {
	return _autoindex;
}
int					Location::getReturnCode() const {
	return _return_code;
}
const std::string&	Location::getReturnUrl() const {
	return _return_url;
}
const std::map<std::string, std::string>&	Location::getCgi() const {
	return _cgi;
}
const std::vector<Location>&				Location::getLocations() const {
	return _locations;
}
const std::string&	Location::getClientMaxBodySize() const {
	return _client_max_body_size;
}

void				Location::print() const {
	std::cout << "    Location: " << _path << std::endl;
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
			_locations[i].print();
		}
	}
}

// --- Server Implementation ---

Server::Server() {
	_port = 8080;
	_host = "127.0.0.1";
	_server_names.push_back("localhost");
	_server_names.push_back("127.0.0.1");
	_root = "www/web";
	_index = "index.html";
	_error_pages[404] = "/var/www/errors/404.html";
	_client_max_body_size = "1m";
	_listen_fd = -1;
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
{ }

Server::~Server() {
	if (_listen_fd != -1) {
		close(_listen_fd);
	}
}

int	Server::setupServer() {
	_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listen_fd == -1) {
		Logger::logErrno(LOG_ERROR, "Failed to create socket");
		return -1;
	}

	int	lflags = fcntl(_listen_fd, F_GETFL, 0);
	if (lflags != -1)
		fcntl(_listen_fd, F_SETFL, lflags | O_NONBLOCK);

	int yes = 1;
	//TODO: REPLACE SO_REUSEADDR WITH SO_KEEPALIVE
	if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
	{
		Logger::logErrno(LOG_ERROR, "Failed to set SO_REUSEADDR");
	}

	struct sockaddr_in	server_address = {};
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(_host.c_str());
	server_address.sin_port = htons(_port);
	_server_address = server_address;

	if (bind(_listen_fd, (struct sockaddr *)&_server_address, sizeof(_server_address)) == -1)
	{
		std::stringstream ss;
		ss << _port;
		Logger::logErrno(LOG_ERROR, "Failed to bind socket to " + _host + ":" + ss.str());
		close(_listen_fd);
		_listen_fd = -1;
		return -1;
	}
	if (listen(_listen_fd, 10) == -1) {
		Logger::logErrno(LOG_ERROR, "Failed to listen on socket");
		return -1;
	}
	Logger::log(LOG_INFO, "Server listening on " + _host + ":" + toString(_port));
	return 0;
}

void	Server::setPort(int port) {
	_port = port;
}
void	Server::setHost(const std::string& host) {
	_host = host;
}
void	Server::addServerName(const std::string& name) {
	_server_names.push_back(name);
}
void	Server::setRoot(const std::string& root) {
	_root = root;
}
void	Server::setIndex(const std::string& index) {
	_index = index;
}
void	Server::addErrorPage(int code, const std::string& page) {
	_error_pages[code] = page;
}
void	Server::addLocation(const Location& location) {
	_locations.push_back(location);
}
void	Server::setClientMaxBodySize(const std::string& size) {
	_client_max_body_size = size;
}
void	Server::addAllowedMethod(const std::string& method) {
	_allowed_methods.push_back(method);
}

int Server::getPort() const {
	return _port;
}

const std::string& Server::getHost() const {
	return _host;
}

size_t	Server::getLocationCount() const {
	return _locations.size();
}
const std::vector<Location>&		Server::getLocations() const {
	return _locations;
}
const std::vector<std::string>&		Server::getServerNames() const {
	return _server_names;
}
const std::map<int, std::string>&	Server::getErrorPages() const {
	return _error_pages;
}
const std::string&					Server::getClientMaxBodySize() const {
	return _client_max_body_size;
}
const std::vector<std::string>&		Server::getAllowedMethods() const {
	return _allowed_methods;
}
int					Server::getListenFd() const {
	return _listen_fd;
}
std::string			Server::getRoot() const {
	return _root;
}
const std::string&	Server::getIndex() const {
	return _index;
}

void	Server::print() const {
	std::cout << "Server Configuration:" << std::endl;
	std::cout << "  Port: " << _port << std::endl;
	std::cout << "  Host: " << _host << std::endl;
	if (!_server_names.empty()) {
		std::cout << "  Server names: ";
		for (size_t i = 0; i < _server_names.size(); i++) {
			std::cout << _server_names[i] << (i < _server_names.size() - 1 ? ", " : "");
		}
		std::cout << std::endl;
	}
	if (!_root.empty()) std::cout << "  Root: " << _root << std::endl;
	if (!_index.empty()) std::cout << "  Index: " << _index << std::endl;
	if (!_allowed_methods.empty()) {
		std::cout << "  Methods: ";
		for (size_t i = 0; i < _allowed_methods.size(); i++) {
			std::cout << _allowed_methods[i] << (i < _allowed_methods.size() - 1 ? ", " : "");
		}
		std::cout << std::endl;
	}
	if (!_client_max_body_size.empty()) {
		std::cout << "  Client max body size: " << _client_max_body_size << std::endl;
	}
	if (!_error_pages.empty()) {
		std::cout << "  Error pages:" << std::endl;
		for (std::map<int, std::string>::const_iterator it = _error_pages.begin(); it != _error_pages.end(); ++it) {
			std::cout << "    " << it->first << " -> " << it->second << std::endl;
		}
	}
	if (!_locations.empty()) {
		std::cout << "  Locations:" << std::endl;
		for (size_t i = 0; i < _locations.size(); i++) {
			_locations[i].print();
		}
	}
}
