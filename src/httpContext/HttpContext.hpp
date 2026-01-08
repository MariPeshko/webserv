#ifndef HTTPCONTEXT_HPP
#define HTTPCONTEXT_HPP

#include "../../inc/Webserv.hpp"
#include "../response/Response.hpp"
#include "../request/Request.hpp"
#include "../server/Server.hpp"
#include "../server/Location.hpp"
#include "../httpContext/Connection.hpp"
#include "HttpParser.hpp"
#include "PrintUtils.hpp"

#include <sys/socket.h>
#include <netinet/in.h>

#define CTX_DEBUG 0
#define REQ_DEBUG 0
#define REQ_DEBUG_BODY 0
#define RESP_DEBUG 0
#define YELLOW "\033[33m"
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define BLUE "\033[34m"
#define RED "\033[31m"
#define ORANGE "\033[38;5;208m"

#define MAX_REQUEST_LINE_SIZE 100      // 100 bytes for request line
#define MAX_HEADER_BLOCK_SIZE 16384     // 16KB for all headers combined

/**
 * Briefly: HTTP (request) state + Request/Response
 *
 * HTTP parsing state machine over Connection’s buffer,
 * owns Request/Response, Parser class has static methods,
 * produces Response when complete.
 */
class HttpContext
{

	public:
		HttpContext(Connection &conn, Server &server);
		HttpContext(const HttpContext &other);
		~HttpContext();

		Connection &connection();
		Server &server();
		// static functions of HttpParser class
		Request &request();
		Response &response();

		enum e_parse_state
		{
			REQUEST_LINE,
			READING_HEADERS,
			READING_FIXED_BODY,	  // For Content-Length
			READING_CHUNKED_BODY, // For Transfer-Encoding
			REQUEST_COMPLETE,
			REQUEST_ERROR
		};

		void	requestParsingStateMachine();
		bool	findAndParseReqLine(std::string &buf);
		bool	findAndParseHeaders(std::string &buf);
		bool	isBodyToRead();
		bool	findAndParseFixBody(std::string &buf);
		bool	chunkedBodyStateMachine(std::string &buf);

		bool	isRequestComplete() const;
		bool	isRequestError() const;
		void	resetState();
		void	buildResponseString();
		e_parse_state	getParserState() const;

		// response sending helpers
		std::string getResponseBuffer() const;
		size_t		getBytesSent() const;
		void		setResponseBuffer(const std::string &buffer);
		void		addBytesSent(size_t bytes);
		bool		isResponseComplete() const;

		// Draining helpers (to safely close after error responses)
		void		startDraining();
		void		stopDraining();
		bool		isDraining() const;
		bool		hasDrainTimedOut(time_t now, time_t timeoutSec) const;

	private:
		HttpContext();									  // no default construction
		HttpContext &operator=(const HttpContext &other); // no assignment

		Connection		_conn;
		Server&			_server_config;
		Request			_request;
		Response		_response;
		e_parse_state	_state;

		bool			validateHost();
		bool			checkRequestLineSize(const std::string &buf);
   		bool			checkHeaderBlockSize(const std::string &buf);
		bool			checkBodySizeLimit(size_t contentLength);
		const Location*	findMatchingLocation();

		// body members
		size_t			_expectedBodyLen;
		enum			e_chunk_state
		{
			READING_CHUNK_SIZE,
			READING_CHUNK_DATA,
			READING_CHUNK_TRAILER // For the final CRLF after a chunk
		};
		e_chunk_state	_chunkState;
		size_t			_chunkSize;
		size_t			_accumulatedBodySize; // for chunk body

		// For non-blocking response sending
		std::string		_responseBuffer;
		size_t			_bytesSent;

		// Draining state
		bool			_draining;
		time_t			_drainStart;
};

#endif
