#ifndef SERVER_HPP
# define SERVER_HPP

# include <iostream>
# include <string>
# include <vector>
# include <map>
# include <sys/types.h>		// socket(), bind(), listen()
# include <sys/socket.h>	// socket(), bind(), listen()
# include <netinet/in.h>	// do we need it here?
# include <fcntl.h>
# include "Location.hpp"
# include "../response/utils.hpp"

#define CONF_DEBUG 0

class	Server {
	public:
		Server();
		Server(const Server& other);
		~Server();
		
		int		setupServer();
		
		// Setters
		void	setPort(int port);
		void	setHost(const std::string& host);
		void	addServerName(const std::string& name);
		void	setRoot(const std::string& root);
		void	setIndex(const std::string& index);
		void	addErrorPage(int code, const std::string& page);
		void	addLocation(const Location& location);
		void	setClientMaxBodySize(const std::string& size);
		void	addAllowedMethod(const std::string& method);
		
		// Getters
		size_t								getLocationCount() const;
		const std::vector<Location>&		getLocations() const;
		const std::vector<std::string>&		getServerNames() const;
		const std::string&					getFirstServerName() const;
		const std::map<int, std::string>&	getErrorPages() const;
		const std::string&					getClientMaxBodySize() const;
		const std::vector<std::string>&		getAllowedMethods() const;
		int									getListenFd() const;
		int									getPort() const;
		const std::string&					getHost() const;
		std::string							getRoot() const;
		const std::string&					getIndex() const;
		
		void print() const;

	private:
		int							_port;
		std::string					_host;
		std::vector<std::string>	_server_names;
		std::string					_root;
		std::string					_index; // bool _autoindex;
		std::map<int, std::string>	_error_pages;
		std::vector<Location>		_locations;
		std::string					_client_max_body_size; // unsigned long
		std::vector<std::string>	_allowed_methods;
		struct sockaddr_in			_server_address;
		int							_listen_fd;
};

#endif