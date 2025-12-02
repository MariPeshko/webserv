#include "Response.hpp"

// Parametric constructor
Response::Response(Server& server)
	: _server_config(server),
	  _request(0),
	  _statusCode(200),
	  _reasonPhrase(generateStatusMessage(200)),
	  _contentLength(0)
{ }

Response&	Response::operator= (const Response & other) {
	if (this != &other) {
		_server_config = other._server_config;
		_request = other._request;
		_statusCode = other.getStatusCode();
		_reasonPhrase = other._reasonPhrase;
		_contentLength = other.getContentLength();
	}
	return *this;
}

Response::~Response() { }

// call after parsing
void Response::bindRequest(const Request& req) {
	_request = &req;
}

// Helper to find the best matching location
// Returns a pointer to the Location object, or NULL if none found
const Location* Response::matchPathToLocation() {
	if (!_request) {
		return NULL;
	}
	const std::vector<Location>& locations = _server_config.getLocations();
	const Location* bestMatch = NULL;
	size_t bestMatchLen = 0;

	if (DEBUG) std::cout << GREEN << "Matching URI: [" << _request->getUri() << "] against " << locations.size() << " locations." << RESET << std::endl;

	for (size_t i = 0; i < locations.size(); ++i) {
		const std::string& locPath = locations[i].getPath();
		if (DEBUG) std::cout << GREEN << "  Checking location: [" << locPath << "]" << RESET << std::endl;
		
		// Check if request URI starts with this location path
		if (_request->getUri() == locPath) {
			if (DEBUG) std::cout << GREEN << "    -> Match found!" << RESET << std::endl;
			// We want the longest match (e.g. location /images/ vs location /)
			if (locPath.length() > bestMatchLen) {
				bestMatch = &locations[i];
				bestMatchLen = locPath.length();
				if (DEBUG) std::cout << GREEN << "    -> New best match (length " << bestMatchLen << ")" << RESET << std::endl;
			}
		}
	}
	return bestMatch;
}

// returns the index file name for the matched location or an empty string if none found
std::string Response::getIndexFromLocation() {
	for (std::vector<Location>::const_iterator it = _server_config.getLocations().begin(); 
			it != _server_config.getLocations().end(); ++it) {
				if (DEBUG) {
					std::cout << "Checking location for index: " << it->getPath() << std::endl;
					std::cout << ORANGE << "Request URI: " << _request->getUri() << RESET << std::endl;
					std::cout << ORANGE << "Location Path: " << it->getPath() << RESET << std::endl;
					std::cout << ORANGE << "Location Index: " << it->getIndex() << RESET << std::endl;
				}
		if (_request->getUri() == it->getPath()) {
			return "/" + it->getIndex();
		}
	}
	return "";
}

void Response::generateResponse() {
	if (!_request) return;

	const Location* loc = matchPathToLocation();
	if (!loc) {
		if (DEBUG) std::cout << RED << "No location matched." << RESET << std::endl;
		_statusCode = 404;
		_reasonPhrase = generateStatusMessage(_statusCode);
		_responseBody = getErrorPageContent(_statusCode);
		_contentLength = _responseBody.size();
		return;
	}

	// Check for allowed methods
	const std::vector<std::string>& allowed = loc->getAllowedMethods();
	if (!allowed.empty()) {
		bool methodAllowed = false;
		for (size_t i = 0; i < allowed.size(); ++i) {
			if (allowed[i] == _request->getMethod()) {
				methodAllowed = true;
				break;
			}
		}
		if (!methodAllowed) {
			if (DEBUG) std::cout << RED << "Method " << _request->getMethod() << " not allowed for this location." << RESET << std::endl;
			_statusCode = 405;
			_reasonPhrase = generateStatusMessage(_statusCode);
			_responseBody = getErrorPageContent(_statusCode);
			_contentLength = _responseBody.size();
			return;
		}
	}

	// Check for redirection
	if (loc->getReturnCode() != 0) {
		_statusCode = loc->getReturnCode();
		_reasonPhrase = generateStatusMessage(_statusCode);
		_headers["Location"] = loc->getReturnUrl();
		_responseBody = "<html><body><h1>" + toString(_statusCode) + " " + _reasonPhrase + "</h1></body></html>";
		_contentLength = _responseBody.size();
		return;
	}

	if (_request->getEnumMethod() == Request::GET) {
		// All the logic for serving files and directories.
		// Construct Path
		std::string root = !loc->getRoot().empty() ? loc->getRoot() : _server_config.getRoot();
		if (DEBUG) std::cout << YELLOW << "Using root: " << root << RESET << std::endl;
		std::string uri = _request->getUri();
		if (DEBUG) std::cout << YELLOW << "Using URI: " << uri << RESET << std::endl;
		std::string path;
		path = root + getIndexFromLocation();
		if (DEBUG) std::cout << GREEN << "Resolved path: " << path << RESET << std::endl;

		// Handle Directory
		if (isDirectory(path)) {		
			if (DEBUG) std::cout << BLUE << "Path is a directory." << RESET << std::endl;
			if (path[path.length() - 1] != '/') path += "/";
			
			// Check for index file
			std::string indexFile = !loc->getIndex().empty() ? loc->getIndex() : "index.html";
			std::string indexPath = path + indexFile;
			
			if (DEBUG) std::cout << BLUE << "Checking index file: " << indexPath << RESET << std::endl;
			if (DEBUG) std::cout << YELLOW << "Index to use: " << loc->getIndex() << RESET << std::endl;

			if (!loc->getIndex().empty()){
				path = indexPath; // Index exists, serve this file
				if (DEBUG) std::cout << GREEN << "Index file found: " << path << RESET << std::endl;
			} else {
				if (DEBUG) std::cout << BLUE << "Index file not found." << RESET << std::endl;
				// No index file found. Check Autoindex.
				if (loc->getAutoindex()) {
					if (DEBUG) std::cout << BLUE << "Autoindex is ON. Generating listing for: " << path << RESET << std::endl;
					std::string body;
					size_t bodyLen = 0;
					if (buildHtmlIndexTable(path, body, bodyLen) == 0) {
						_statusCode = 200;
						_reasonPhrase = generateStatusMessage(_statusCode);
						_responseBody = body;
						_contentLength = bodyLen;
						return;
					}
					if (DEBUG) std::cout << RED << "Autoindex generation failed." << RESET << std::endl;
					_statusCode = 500;
					_reasonPhrase = generateStatusMessage(_statusCode);
					_responseBody = getErrorPageContent(_statusCode);
					_contentLength = _responseBody.size();
					return;
				} else {
					// Directory, no index, no autoindex -> Forbidden
					if (DEBUG) std::cout << RED << "Directory access forbidden (no index, autoindex off)" << RESET << std::endl;
					_statusCode = 403;
					_reasonPhrase = generateStatusMessage(_statusCode);
					_responseBody = getErrorPageContent(_statusCode);
					_contentLength = _responseBody.size();
					return;
				}
			}
		}

		// 3. Serve File
		if (DEBUG) std::cout << BLUE << "Serving file: " << path << RESET << std::endl;
		std::ifstream file(path.c_str());
		if (!file.is_open()) {
			if (DEBUG) std::cout << RED << "File not found: " << path << RESET << std::endl;
			_statusCode = 404;
			_reasonPhrase = generateStatusMessage(_statusCode);
			_responseBody = getErrorPageContent(_statusCode);
			_contentLength = _responseBody.size();
			return;
		}

		std::ostringstream ss;
		ss << file.rdbuf();
		_responseBody = ss.str();
		_contentLength = _responseBody.size();
		file.close();
	} else if (_request->getEnumMethod() == Request::POST) {
		std::cout << BLUE << "POST method" << RESET << std::endl;
		// TODO: Add your logic for handling POST requests here.
		// For example, processing form data or file uploads.
		// You might set status code to 201 (Created) or 200 (OK).
		_statusCode = 501; // Not Implemented
		_reasonPhrase = generateStatusMessage(_statusCode);
		_responseBody = getErrorPageContent(_statusCode);
		_contentLength = _responseBody.size();
	} else if (_request->getEnumMethod() == Request::DELETE) {
		// TODO: Add your logic for handling DELETE requests here.
		// For example, deleting a resource from the server.
		// You might set status code to 200 (OK) or 204 (No Content).
		_statusCode = 501; // Not Implemented
		_reasonPhrase = generateStatusMessage(_statusCode);
		_responseBody = getErrorPageContent(_statusCode);
		_contentLength = _responseBody.size();
	} else if (_request->getEnumMethod() == Request::INVALID) {
		_statusCode = 501; // Not Implemented
		_reasonPhrase = generateStatusMessage(_statusCode);
		_responseBody = getErrorPageContent(_statusCode);
		 _contentLength = _responseBody.size();
	}

}

short Response::getStatusCode() const { return _statusCode; }

size_t Response::getContentLength() const { return _contentLength; }

std::string Response::getResponseBody() const {
	return _responseBody;
}

/** nulls the Request* before new HTTP request-response cycle */
void Response::reset() {
	_request = 0;
	_statusCode = 200;
	_reasonPhrase = generateStatusMessage(_statusCode);
	_responseBody.clear();
	_contentLength = 0;
}

Server& Response::getServerConfig() {
	return _server_config;
}

std::string Response::getReasonPhrase() const {
	return _reasonPhrase;
}

const std::map<std::string, std::string>& Response::getHeaders() const {
	return _headers;
}

std::string Response::getErrorPageContent(int code) {
	const std::map<int, std::string>& errorPages = _server_config.getErrorPages();
	std::map<int, std::string>::const_iterator it = errorPages.find(code);
	
	if (it != errorPages.end()) {
		std::ifstream file(it->second.c_str());
		if (file.is_open()) {
			std::ostringstream ss;
			ss << file.rdbuf();
			return ss.str();
		}
	}
	return "";
}