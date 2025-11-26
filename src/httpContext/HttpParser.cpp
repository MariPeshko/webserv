#include "HttpParser.hpp"

HttpParser::HttpParser() { }

HttpParser::~HttpParser() { }

bool HttpParser::parseRequestLine(const std::string& line, 
									Request& req) {
	if (line.empty())
	{
		req.setRequestLineFormatValid(false);
		req.setMethod("");
		return false;
	}
	std::istringstream			iss(line); 
	std::string					method, uri, version;

	if (!(iss >> method >> uri >> version) || !iss.eof()) {
		req.setRequestLineFormatValid(false);
		req.setMethod("");
		return false;
	}
	req.setRequestLineFormatValid(true);

	if (method != "GET" && method != "POST" && method != "DELETE") {
		req.setRequestLineFormatValid(false);
		req.setMethod("");
		return false;
	}

	if (version != "HTTP/1.1" && version != "HTTP/1.0") {
		req.setRequestLineFormatValid(false);
		return false;
	}

	// Rudimentary URI check
	if (uri.empty() || uri[0] != '/' || uri.find("..") != std::string::npos) {
		req.setRequestLineFormatValid(false);
		return false;
	}

	req.setMethod(method);
	req.setUri(uri);
	req.setVersion(version);
	return true;
}

bool HttpParser::parseHeaders(const std::string& line,
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
