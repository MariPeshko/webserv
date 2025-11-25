#include "HttpParser.hpp"

HttpParser::HttpParser() { }

HttpParser::~HttpParser() { }

bool HttpParser::parseRequestLine(const std::string& line, 
									Request& req) {
	(void)line;
	(void)req;
	return true;
}

bool HttpParser::parseHeaderLine(const std::string& line,
									Request& req) {
	if (line.empty())
		return true; // blank line handled upstream
	size_t	colon = line.find(':');
	if (colon == std::string::npos)
		return false;
	std::string	name = line.substr(0, colon);
	std::string	value = line.substr(colon + 1);
	// trim leading spaces
	while (!value.empty() && 
			(value[0] == ' ' || value[0] == '\t'))
		value.erase(0,1);
	req.addHeader(name, value);
	return true;
}
