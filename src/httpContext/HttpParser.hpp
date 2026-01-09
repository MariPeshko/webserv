#ifndef HTTPPARSER_HPP
# define HTTPPARSER_HPP

# include "../../inc/Webserv.hpp"
# include "../request/Request.hpp"
# include "PrintUtils.hpp"
# include <limits> // C++98: for std::numeric_limits<size_t>::max()
# include <cstdio>

#define DEBUG_HTTP_PARSER 0

/**
 *  Stateless syntax helpers for HTTP request.
 */
class	HttpParser {

	public:
		HttpParser();
		~HttpParser();

	static bool			parseRequestLine(const std::string& line, Request& req);
	static bool			parseHeaders(const std::string& headersBlock, Request& req);
	static void			appendToBody(const std::string & buffer, const size_t n, Request& req);
	static bool			cpp98_hexaStrToInt(const std::string& s, size_t& out);
	static bool			parseMultipartData(const std::string& reqBody, const std::string& boundary, 
							std::string& filename, std::string& fileData);
	static std::string	getExtensionStr(const std::string& filename);
	static bool			parseMultiHeadersName(std::string& multipart_headers, std::string& filename);
	static std::string	extractBoundary(const std::string& contentType);
	static bool			isExtensionAllowed(const std::string& filename);
	static size_t		parseSizeString(const std::string& sizeStr);
	static bool			safeParseContentLength(const std::string &cl, size_t &contentLength);

	private:
};

#endif
