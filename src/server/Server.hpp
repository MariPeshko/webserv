#ifndef SERVER_HPP
# define SERVER_HPP

# include <iostream>
# include <string>
# include <vector>
# include <map>
# include <sys/types.h>		// socket(), bind(), listen()
# include <sys/socket.h>	// socket(), bind(), listen()
# include <netinet/in.h>	// do we need it here?

// Ivan -> Alberto

class Location {
	public:
		Location() : _autoindex(false), _return_code(0) {};
		~Location() {};
		
		// Setters for the parser
		void setPath(const std::string& path) { _path = path; }
		void setRoot(const std::string& root) { _root = root; }
		void setIndex(const std::string& index) { _index = index; }
		void setAutoindex(bool autoindex) { _autoindex = autoindex; }
		void addAllowedMethod(const std::string& method) { _allowed_methods.push_back(method); }
		void setReturn(int code, const std::string& url) { _return_code = code; _return_url = url; }
		void addCgi(const std::string& ext, const std::string& path) { _cgi[ext] = path; }
		
		// Getters for debugging
		const std::string& getPath() const { return _path; }
		const std::string& getRoot() const { return _root; }
		const std::vector<std::string>& getAllowedMethods() const { return _allowed_methods; }
		const std::string& getIndex() const { return _index; }
		bool getAutoindex() const { return _autoindex; }
		int getReturnCode() const { return _return_code; }
		const std::string& getReturnUrl() const { return _return_url; }
		const std::map<std::string, std::string>& getCgi() const { return _cgi; }
		
		void print() const {
			std::cout << "    Location: " << _path << std::endl;
			if (!_root.empty()) std::cout << "      root: " << _root << std::endl;
			if (!_allowed_methods.empty()) {
				std::cout << "      methods: ";
				for (size_t i = 0; i < _allowed_methods.size(); i++) {
					std::cout << _allowed_methods[i];
					if (i < _allowed_methods.size() - 1) std::cout << ", ";
				}
				std::cout << std::endl;
			}
			if (!_index.empty()) std::cout << "      index: " << _index << std::endl;
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
		}

		std::string	_path;
	private:
		std::string _root;
		std::vector<std::string>	_allowed_methods;
		std::string	_index;
		bool	_autoindex;
		int		_return_code;
		std::string _return_url;
		std::map<std::string, std::string> _cgi;
};

class	Server {
    public:
		Server(const Server& other);
        Server();
        ~Server();
		
		int		setupServer();
        
        // Setters for parser
        void	setPort(int port) { _port = port; }
        void	setHost(const std::string& host) { _host = host; }
        void	addServerName(const std::string& name) { _server_names.push_back(name); }
        void	setRoot(const std::string& root) { _root = root; }
        void	setIndex(const std::string& index) { _index = index; }
        void	addErrorPage(int code, const std::string& page) { _error_pages[code] = page; }
        void	addLocation(const Location& location) { _locations.push_back(location); }
        void	setClientMaxBodySize(const std::string& size) { _client_max_body_size = size; }
        
        // Getters for debugging/testing
        size_t								getLocationCount() const { return _locations.size(); }
        const std::vector<Location>&		getLocations() const { return _locations; }
        const std::vector<std::string>&		getServerNames() const { return _server_names; }
        const std::map<int, std::string>&	getErrorPages() const { return _error_pages; }
        const std::string&					getClientMaxBodySize() const { return _client_max_body_size; }
		int									getListenFd() const { return _listen_fd; }
        
        void print() const {
			std::cout << "Server Configuration:" << std::endl;
			std::cout << "  Port: " << _port << std::endl;
			std::cout << "  Host: " << _host << std::endl;
			if (!_server_names.empty()) {
				std::cout << "  Server names: ";
				for (size_t i = 0; i < _server_names.size(); i++) {
					std::cout << _server_names[i];
					if (i < _server_names.size() - 1) std::cout << ", ";
				}
				std::cout << std::endl;
			}
			/* if (!_client_max_body_size.empty()) {
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
			} */
		}

    private:
        int							_port; // ?uint16_t
        std::string					_host; // ?in_addr_t
        std::vector<std::string>	_server_names;
        std::string					_root;
        std::string					_index; // bool _autoindex;
        std::map<int, std::string>	_error_pages;
        std::vector<Location>		_locations;
        std::string					_client_max_body_size; // unsigned long
		// members of Ivan's class ServerConfig
		struct sockaddr_in			_server_address;
        int							_listen_fd;
};

#endif