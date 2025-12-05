#ifndef HTTPCONTEXT_HPP
# define HTTPCONTEXT_HPP

# include "../response/Response.hpp"
# include "HttpParser.hpp"
# include "../request/Request.hpp"
# include "../server/Server.hpp"
#include "../httpContext/Connection.hpp"
# include <iostream>
# include <string>
# include <vector>
# include <map>
# include <sys/socket.h>
# include <netinet/in.h>
# include <sstream>

/**
 * Briefly: HTTP (request) state + Request/Response
 * 
 * HTTP parsing state machine over Connection’s buffer, 
 * owns Request/Response, Parser class has static methods, 
 * produces Response when complete.
 */
class	HttpContext {

	public:
		HttpContext(Connection& conn, Server& server);
		HttpContext(const HttpContext &other);
		~HttpContext();

		Connection&		connection();
		Server&			server();
		//static functions of HttpParser class
		Request&		request();
		Response&		response();

		enum	e_parse_state {
			REQUEST_LINE,
			READING_HEADERS,
			READING_BODY,
			REQUEST_COMPLETE,
			REQUEST_ERROR
		};		
		
		void	parseRequest();
		void	parseHeaders(std::string headers);
		void	parseBody(std::string body);

		bool	isRequestComplete() const;
		bool	isRequestError() const;
		
		void		buildResponseString();
		void	resetState();
		
		// getters
		e_parse_state	getParserState() const;

		// Response buffer for POLLOUT
		std::string		getResponseBuffer() const;
		size_t			getBytesSent() const;
		void			setResponseBuffer(const std::string& buffer);
		void			addBytesSent(size_t bytes);
		bool			isResponseComplete() const;

	private:
		HttpContext();	// no default construction
		HttpContext& operator=(const HttpContext& other);  // no assignment

		Connection	_conn;
		Server&		_server_config;
		Request		_request;
		Response	_response;

		e_parse_state	_state;
		size_t			_expectedBodyLen;
		
		// For non-blocking response sending
		std::string		_responseBuffer;
		size_t			_bytesSent;
};

#endif