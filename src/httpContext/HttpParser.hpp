#ifndef HTTPPARSER_HPP
# define HTTPPARSER_HPP

# include "../request/Request.hpp"
# include <string>
# include <map>
# include <vector>
# include <iostream>
# include <sstream>
#include <limits>

/**
 *  Stateless syntax helpers for HTTP request.
 */
class	HttpParser {

	public:
		HttpParser();
		~HttpParser();

	static bool parseRequestLine(const std::string& line, Request& req);
    static bool parseHeaders(const std::string& headersBlock, Request& req);
	static void	appendToBody(const std::string & buffer, const size_t n, Request& req);
    static bool parseFixBody(const std::string& bodyBlock, Request& req);
	static bool	cpp98_hexaStrToInt(const std::string& s, size_t& out); 

	private:
};



	

#endif