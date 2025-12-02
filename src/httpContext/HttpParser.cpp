#include "HttpParser.hpp"
#include "PrintUtils.hpp"
#include <limits>

HttpParser::HttpParser() { }

HttpParser::~HttpParser() { }

void	HttpParser::appendToBody(const std::string & buffer, const size_t n, Request& req) {
	if (n == 0 || buffer.empty()) {
		return;
	}
	 // Assumes Request::getBody() returns a non-const std::string&
	req.getBody().append(buffer, 0, n);
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
	req.setMethod(method);
	req.setUri(uri);
	req.setVersion(version);
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

// helper. validates and converts a hex string to size_t
// 
// updating the 'value' in each loop iteration.
// '10u' is the unsigned integer literal 10.
bool	HttpParser::cpp98_hexaStrToInt(const std::string& s, size_t& out) {
	if (s.empty())
		return false;

	size_t	value = 0;
	for (size_t i = 0; i < s.size(); ++i) {
		const char	c = s[i];
		size_t		hex_digit; // 0..15

		if (c >= '0' && c <= '9') {
			hex_digit = static_cast<size_t>(c - '0');
		} else if (c >= 'a' && c <= 'f') {
			hex_digit = 10u + static_cast<size_t>(c - 'a');
		} else if (c >= 'A' && c <= 'F') {
			hex_digit = 10u + static_cast<size_t>(c - 'A');
		} else {
			return false; // invalid character
		}

		// Prevent overflow: (value * 16) must not exceed size_t max
		const size_t	max_before_mul = std::numeric_limits<size_t>::max() / 16u;
		if (value > max_before_mul)
			return false;
		const size_t	v16 = value * 16u; // to base-16
		// Prevent overflow on addition
		if (hex_digit > std::numeric_limits<size_t>::max() - v16)
			return false;

		value = v16 + hex_digit;
	}

	out = value;
	return true;
}
