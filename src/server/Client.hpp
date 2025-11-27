#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "../response/Response.hpp"
# include "../request/HttpParser.hpp"
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
		Client(Server &server);
		Client(const Client &other);
		~Client();

		HttpParser&		parser();
		Request&		request(); // Data object that holds parsed request
		enum	e_parse_state {
			REQUEST_LINE,
			READING_HEADERS,
			READING_BODY,
			REQUEST_COMPLETE,
			REQUEST_ERROR
		};

		Response	response;
		Server&		server_config;
		
		ssize_t	receiveData();
		void	parseRequest();
		void	parseRequestLine(std::string request_line);
		void	parseHeaders(std::string headers);
		void	parseBody(std::string body);

		bool	isRequestComplete() const;
		bool	isRequestError() const;

		// setters
		void	setFd(int fd);
		void	setClientAddress(const sockaddr_in& client_address);
		void	resetState();
		
		// getters
		std::string		getResponseString();
		int				getFd() const;
		std::string &	getBuffer();
		e_parse_state	getParserState() const;

	private:
		Client();	// no default construction
		Client& operator=(const Client& other);  // no assignment

		HttpParser 			_parser;
		Request				_request;

		int 				_fd;
		struct sockaddr_in	_client_address;
		std::string			_request_buffer;
		e_parse_state		_state;
};

#endif