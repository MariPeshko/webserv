#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "../response/Response.hpp"
# include "../request/Request.hpp"
# include <iostream>
# include <string>
# include <vector>
# include <map>
# include <sys/socket.h>
# include <netinet/in.h>
# include <sstream>

class	Client {
	public:
		Client();
		Client(const Client &other);
		~Client();
		
		void		setFd(int fd);
		void		setClientAddress(const sockaddr_in& client_address);
		
		std::string	getResponseString();
		int			getFd();
		
		Request		request; 
		Response	response;
	
	private:
		int 				_fd;
		struct sockaddr_in	_client_address;
		std::string			_request_buffer;
};

#endif