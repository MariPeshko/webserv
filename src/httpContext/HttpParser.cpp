#include "HttpParser.hpp"
#include "PrintUtils.hpp"

HttpParser::HttpParser() { }

HttpParser::~HttpParser() { }

bool	HttpParser::parseBody(const std::string& bodyBlock, Request& req) {
	(void)bodyBlock;
	(void)req;
	return true;
}


/**
 * Parser for a request line of the request.
 * 
 * URI check
 * uri.find("..") - a security check to prevent directory traversal attacks.
 * ".." is used to go up one directory. An attacker could craft a URI 
 * like passwd to try and access sensitive files outside of the 
 * web server's intended root directory.
 */
bool HttpParser::parseRequestLine(const std::string& line, 
									Request& req) {
	if (line.empty()) {
		req.setRequestLineFormatValid(false);
		req.setMethod("");
		return false;
	}
	std::istringstream	iss(line); 
	std::string			method, uri, version;

	// false if the stream iss fails to extract all three strings.
	if (!(iss >> method >> uri >> version) || !iss.eof()) {
		req.setRequestLineFormatValid(false);
		req.setMethod("");
		return false;
	}

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
	req.setRequestLineFormatValid(true);
	return true;
}

/**
 * @brief Parses a block of HTTP headers.
 * 
 * This function takes a string containing all header lines, splits them,
 * and parses each one individually. It normalizes header names to lowercase
 * to ensure case-insensitive handling.
 * 
 * Line must end with a carriage return followed by a line feed (\r\n).
 * 
 * @param headersBlock The string containing the entire header section.
 * @param req The Request object to populate with headers.
 * @return true if all headers were parsed successfully, false otherwise.
 */
bool HttpParser::parseHeaders(const std::string& headersBlock,
									Request& req) {
	if (headersBlock.empty()) {
		req.setHeadersFormatValid(false);
		return false;
	}

	std::istringstream	iss(headersBlock);
	std::string 		line;

	while(std::getline(iss, line)) {

		// Handle potential carriage return at the end of the line
		if (!line.empty() && line[line.length() - 1] == '\r') {
			line.erase(line.length() - 1);
		}

		if(line.empty()) continue;

		size_t	pos_colon = line.find(':');
		if(pos_colon == std::string::npos) {
			req.setHeadersFormatValid(false);
			return false; // Malformed header line
		}
		
		std::string	name = line.substr(0, pos_colon);
		std::string	value = line.substr(pos_colon +1);

		//Trim leading whitespace from name
		size_t start = name.find_first_not_of(" \t");
		if (start != std::string::npos) {
			name = name.substr(start);
		}
		//Trim trailing whitespace from name
		size_t end = name.find_last_not_of(" \t");
		if (end != std::string::npos) {
			name = name.substr(0, end + 1);
		}

		// Normalize header name to lowercase
		for (size_t i = 0; i < name.length(); ++i) {
			name[i] = std::tolower(name[i]);
		}

		// Trim leading whitespace from value
		start = value.find_first_not_of(" \t");
		if (start != std::string::npos) {
			value = value.substr(start);
		}

		if (name.empty()) {
			return false; // Header name cannot be empty
		}

		req.addHeader(name, value);

	}
	return true;
}
