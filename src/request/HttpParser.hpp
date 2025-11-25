#ifndef HTTPPARSER_HPP
# define HTTPPARSER_HPP

# include "Request.hpp"
# include <string>
# include <map>
# include <vector>
# include <iostream>
# include <sstream>

/**
 *  Stateless syntax helpers for HTTP request.
 */
class	HttpParser {

	public:
		HttpParser();
		~HttpParser();

	static bool parseRequestLine(const std::string& line, Request& req);
    static bool parseHeaderLine(const std::string& line, Request& req);

	static const size_t MAX_REQUEST_LINE  = 8192;      // 8 KB
    static const size_t MAX_HEADER_BLOCK  = 16384;     // 16 KB
    static const size_t MAX_BUFFER_TOTAL  = 1024*1024; // 1 MB fallback

	private:
};



	

#endif