#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "../response/Response.hpp"
# include "../request/Request.hpp"
# include "../server/Server.hpp"
# include <iostream>
# include <string>
# include <vector>
# include <map>
# include <sys/socket.h>
# include <netinet/in.h>
# include <sstream>

class	Client {

	public:
		enum	e_parse_state {
			REQUEST_LINE,
			READING_HEADERS,
			READING_BODY,
			REQUEST_COMPLETE,
			REQUEST_ERROR
		};

		Client(const Client &other);
		Client(Server &server);
		~Client();
		
		ssize_t	receiveData();
		void	parseRequest();
		void	parseRequestLine(std::string request_line);
		void	parseHeaders(std::string headers);
		void	parseBody(std::string body);

		bool	isRequestComplete() const;

		// setters
		void	setFd(int fd);
		void	setClientAddress(const sockaddr_in& client_address);
		void	resetState();
		
		// getters
		std::string		getResponseString();
		int				getFd() const;
		std::string &	getBuffer();
		e_parse_state	getParserState() const;
		
		Request		request; // Data object that holds parsed request
		Response	response;
		Server&		server_config;

	private:
		Client();	// no default construction
		Client& operator=(const Client& other);  // no assignment

		int 				_fd;
		struct sockaddr_in	_client_address;
		std::string			_request_buffer;
		e_parse_state		_state;
};

#endif