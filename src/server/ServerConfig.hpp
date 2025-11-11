#ifndef SERVER_CONFIG_HPP
# define SERVER_CONFIG_HPP

# include <iostream>
# include <string>
# include <vector>
# include <map>
# include <sys/socket.h>
# include <netinet/in.h>
# include "../parser/Location.hpp"


class ServerConfig {
    public:
        ServerConfig();
        ~ServerConfig();
        void setupServer();


    private:
		uint16_t						_port;
		in_addr_t						_host;
		std::string						_server_name;
		std::string						_root;
		unsigned long					_client_max_body_size;
		std::string						_index;
		bool							_autoindex;
		std::map<short, std::string>	_error_pages;
		std::vector<Location> 			_locations;
        struct sockaddr_in 				_server_address;
        int     						_listen_fd;

};
#endif