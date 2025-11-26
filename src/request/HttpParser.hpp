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

	private:
};



	

#endif