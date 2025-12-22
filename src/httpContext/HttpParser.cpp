#include "HttpParser.hpp"
#include "PrintUtils.hpp"
#include <limits>
#include <cstdio>
#include <ctime>    // For time functions
#include <sstream>  // For stringstream

using std::cout;
using std::cerr;
using std::endl;
using std::string;

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
		if(method.size() != 0)
			req.setMethod(method);
		else
			req.setMethod("");
		if(uri.size() != 0) req.setUri(uri);
		if(version.size() != 0) req.setVersion(version);
		req.setRequestLineFormatValid(false);
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

// Generate Timestamp for a filename
static string generateTimestamp() {
	
	time_t				now = time(0);
	struct tm*			tstruct = localtime(&now);
	std::stringstream	timestamp_ss;
	timestamp_ss << (1900 + tstruct->tm_year)
				<< (tstruct->tm_mon + 1 < 10 ? "0" : "") << (tstruct->tm_mon + 1) // a zero-based index; 0 = January
				<< (tstruct->tm_mday < 10 ? "0" : "") << tstruct->tm_mday
				<< "-"
				<< (tstruct->tm_hour < 10 ? "0" : "") << tstruct->tm_hour
				<< (tstruct->tm_min < 10 ? "0" : "") << tstruct->tm_min
				<< (tstruct->tm_sec < 10 ? "0" : "") << tstruct->tm_sec;
	return timestamp_ss.str();
}

bool	HttpParser::parseMultiHeadersName(string& multipart_headers, string& filename)
{
	string	headers = multipart_headers;
	string	original_filename;
	string	sanitized_filename;

	size_t	filename_header = headers.find("filename=\"");
	if (filename_header == std::string::npos) {
		cerr << "Error: malformed multipart header; no \"filename=\"\" found" << endl;
		return false;
	}
	headers.erase(0, filename_header + 10);
	size_t	char_after_name = headers.find("\"");
	if (char_after_name == std::string::npos) {
		cerr << "Error: malformed multipart header; no closure \" found" << endl;
		return false;
	}
	original_filename = headers.substr(0, char_after_name);
	// Sanitize filename
    for (size_t i = 0; i < original_filename.length(); ++i) {
        unsigned char c = original_filename[i];
        if (c < 128) {
            if (c == ' ' || c == '\t') {
                sanitized_filename += '_';
            } else {
                sanitized_filename += c;
            }
		} else {
			sanitized_filename += "non_ascii_name";
			sanitized_filename += HttpParser::getExtensionStr(original_filename);
			break;
		}
    }
	string	timestamp_ss = generateTimestamp();
	// Prepend timestamp to the filename
	filename = timestamp_ss + "_" + sanitized_filename;
	
	return true;
}

// parser for multipart/form-data
bool	HttpParser::parseMultipartData(const string& reqBody, 
			const string& boundary, string& filename,
				string& fileData)
{
	if (boundary.empty()) {
		cerr << "Error: malformed multipart request; no boundary found" << endl;
		return false;
	}

	string buf = reqBody;
	// Multipart boundaries are prefixed with "--" in the body
	string startBoundary = "--" + boundary;
	string endBoundary = "--" + boundary + "--";
	
	size_t	pos_start_headers = reqBody.find(startBoundary);
	if (pos_start_headers == std::string::npos) {
		cerr << "Error: malformed multipart request; no start boundary found" << endl;
		return false;
	}
	buf.erase(0, startBoundary.size() + 2);
	size_t	pos_end_headers = buf.find("\r\n\r\n");
	if (pos_end_headers == string::npos) {
		cerr << "Error: malformed multipart request" << endl;
		return false;
	}
	string	multipart_headers = buf.substr(0, pos_end_headers);

	if (!parseMultiHeadersName(multipart_headers, filename)) {
		return false;
	}
	buf.erase(0, pos_end_headers + 4);
	// The actual file data starts after \r\n\r\n
	size_t pos_body_start = 0;

	// .find() - string Search Operation
	// s2, pos Look for the string s2 starting at position pos
	// in s.pos defaults to 0.
	size_t pos_end_boundary = buf.find(endBoundary, pos_body_start);
	if (pos_end_boundary == string::npos) {
		std::cerr << "Error: malformed multipart request; no end boundary found" << endl;
		return false;
	}
	// Extract file data (subtract 2 for the \r\n before end boundary)
	size_t	file_data_length = pos_end_boundary - 2;
	fileData = buf.substr(0, file_data_length);
	
	string	picture = fileData.substr(0, 10);

	// TO DELETE Expected JPEG structure:
	// Start: FF D8 FF (JPEG header)
	// End: FF D9 (JPEG end marker) + possible metadata
	// %02x is a formatting specifier used with the string formatting 
	// operator % to represent an integer as a two-digit hexadecimal (base-16) number.
	/* cout << "First 10 bytes (JPEG header): ";
	for (size_t i = 0; i < 10 && i < fileData.size(); ++i) {
		printf("%02X ", (unsigned char)fileData[i]);
	}
	cout << endl; */
	/* size_t jpegEnd = fileData.find("\xFF\xD9");
	// Include the FF D9 bytes (add 2)
	string cleanJpeg = fileData.substr(0, jpegEnd + 2);
	cout << "Clean JPEG size: " << cleanJpeg.size() << " bytes" << endl;

	// Verify it ends with FF D9
	cout << "Last 2 bytes should be FF D9: ";
	for (size_t i = std::max(0, (int)cleanJpeg.size() - 2); i < cleanJpeg.size(); ++i) {
		printf("%02X ", (unsigned char)cleanJpeg[i]);
	}
	cout << endl; */
	return true;
}

std::string	HttpParser::extractBoundary(const std::string& contentType) {
	
	size_t	boundaryPos = contentType.find("boundary=");
	if (boundaryPos == std::string::npos) {
		return ""; // Error: no boundary found
	}
	// Skip "boundary="
	std::string boundary = contentType.substr(boundaryPos + 9); 
	
	// Remove any trailing semicolon or whitespace
	size_t	semicolon = boundary.find(';');
	if (semicolon != std::string::npos) {
		boundary = boundary.substr(0, semicolon);
	}
	
	return boundary;
}

string HttpParser::getExtensionStr(const std::string& filename) {
	size_t dot_pos = filename.find_last_of(".");
    if (dot_pos == std::string::npos) {
        return ".no_extention";
    }
	std::string	ext = filename.substr(dot_pos);
	for (size_t i = 0; i < ext.length(); ++i) {
        ext[i] = std::tolower(ext[i]);
    }
	return ext;
}

// Conceptual function to sanitize and validate a filename's extension
bool	HttpParser::isExtensionAllowed(const std::string& filename) {
    // 1. Isolate the extension
    size_t dot_pos = filename.find_last_of(".");
    if (dot_pos == std::string::npos) {
        return false;
    }
    std::string	ext = filename.substr(dot_pos);
    // 2. Normalize to lowercase
    for (size_t i = 0; i < ext.length(); ++i) {
        ext[i] = std::tolower(ext[i]);
    }
    // 3. Whitelist of allowed extensions
    const char*		allowed_extensions[] = {".jpg", ".jpeg", ".png", ".gif"};
    const size_t	num_allowed = sizeof(allowed_extensions) / sizeof(allowed_extensions[0]);
    // 4. Validate against the whitelist
    for (size_t i = 0; i < num_allowed; ++i) {
        if (ext == allowed_extensions[i]) {
            return true;
        }
    }
    return false;
}
