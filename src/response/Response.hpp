#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#define DEBUG 0
#define DEBUG_PATH 0
#define D_POST 0
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

class	Response
{
	public:
		Response(Server &server);
		~Response();

		void			bindRequest(const Request &req);
		void			badRequest();
		void			generateResponse();
		void			generateResponseGet();
		void			generateResponsePost();
		void			generateResponseDelete();
		
		void			fillResponse(short statusCode, const std::string &bodyContent);
		std::string		getErrorPageContent(int code);

		const Request*		getRequest();
		short				getStatusCode() const;
		size_t				getContentLength() const;
		const std::string&	getResponseBody() const;
		const std::string&	getReasonPhrase() const;
		Server&				getServerConfig();
		void				reset();
		const std::map<std::string, std::string>&	getHeaders() const;

		std::string		finalResponseContent;

		enum PathType
		{
			FILE_PATH,
			DIRECTORY_PATH,
			NOT_EXIST
		};

	private:
		Response();
		Response(const Response &);
		Response &operator=(const Response &other);

		Server&				_server_config;
		const Request*		_request;

		short				_statusCode;
		std::string			_reasonPhrase;
		size_t				_contentLength;
		std::string			_responseBody;
		std::string			_resourcePath;
		std::map<std::string, std::string>	_headers;
		std::string			_path;
		const Location*		_loc;

		// main responces methods
		const Location*		validateRequestAndGetLocation();
		std::string			constructPath(const Location* loc);
		bool				tryServeCgi();
		bool				applyCgiOutput(const std::string &output);

		// helpers
		const Location*		matchPathToLocation();
		std::string			getIndexFromLocation();
		PathType			getPathType(std::string const path);
		static std::string	getMimeType(const std::string &filePath);
		std::string			buildCreatedResponse(const std::string& uri, const std::string&filename);

};

#endif
