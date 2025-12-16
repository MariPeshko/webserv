#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#define DEBUG 0
#define D_POST 1
#define GREEN "\033[32m"
#define RESET "\033[0m"
#define BLUE "\033[34m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define ORANGE "\033[38;5;208m"

#include "../request/Request.hpp"
#include "../server/Server.hpp"
#include "utils.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <ctime>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

//TODO:
// - if error - build error response
// - set up status code during response generation (code could be changed due to errors, file not found, etc.)
// - build html response based on request and server config
// - set headers (just append important headers to map<string, string> _headers)
// - add body content

class	Response {
	public:
		
		Response(Server& server);
		~Response();

		void		bindRequest(const Request& req);
		void		generateResponse();
		void		postAndGenerateResponse();
		const Location*	matchPathToLocation();
		const Location*	matchPrefixPathToLocation();
		bool			prefixMatching(const std::string& locPath, const std::string& RequestUri);
		void		reset();

		short		getStatusCode() const;
		size_t		getContentLength() const;
		std::string	getResponseBody() const;
		std::string getReasonPhrase() const; 
		Server&		getServerConfig();
		const std::map<std::string, std::string>& getHeaders() const;

		static std::string	getMimeType(const std::string& filePath);

        std::string finalResponseContent;

	private:
		Response(); // no default construction
		// Maryna's suggestion. No copyable, for safety.
		Response(const Response&);
		Response&	operator= (const Response & other);
	
		// we should check what to store here
		// most of the data will be stored in Request object
		// Status Line
		Server&			_server_config; // reference
		const Request*	_request;		// pointer

		short			_statusCode;
		std::string		_reasonPhrase;
		// Headers
		//not sure if need a map of headers
		// I guess we need them for successful POST method (Maryna)
		std::map<std::string, std::string>	_headers;
		size_t			_contentLength;
		std::string		_responseBody;
		std::string		_resourcePath;

		std::string			getIndexFromLocation();
		std::string			getErrorPageContent(int code);


		//?? for image or binary data response
		// std::vector<uint8_t> _responseBody;
};


#endif