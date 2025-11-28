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

#define YELLOW "\033[33m"
#define RESET "\033[0m"

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
			READING_FIXED_BODY, // For Content-Length
			READING_CHUNKED_BODY, // For Transfer-Encoding
			REQUEST_COMPLETE,
			REQUEST_ERROR
		};		
		
		void	requestParsingStateMachine();
		// findAndParseReqLine()
		bool	findAndParseHeaders(std::string &buf);
		bool	isBodyToRead();

		bool	isRequestComplete() const;
		bool	isRequestError() const;

		void	resetState();
		
		// getters
		std::string		getResponseString();
		e_parse_state	getParserState() const;

	private:
		HttpContext();	// no default construction
		HttpContext& operator=(const HttpContext& other);  // no assignment

		Connection	_conn;
		Server&		_server_config;
		Request		_request;
		Response	_response;

		e_parse_state	_state;
		size_t			_expectedBodyLen;
};

#endif