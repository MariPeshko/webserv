#include "Response.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

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

// Method POST and generation of the response
// To Do refactor code that ised in generateResponse():
// 1. matchPrefixPathToLocation()(); 2. if (!loc) 3. allowed = loc->getAllowedMethods();
// 4. if (!allowed.empty()) 5. Check for redirection if (loc->getReturnCode() != 0) 
void	Response::postAndGenerateResponse() {
	if (D_POST) cout << BLUE << "POST method" << RESET << endl;
	const Location* loc = matchPrefixPathToLocation();
	if (!loc) {
		if (D_POST) cout << RED << "No location matched." << RESET << endl;
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
			if (DEBUG) cout << RED << "Method " << _request->getMethod() << " not allowed for this location." << RESET << endl;
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
	// POST METHOD logic starts here
	// 1. Construct Path
	if (D_POST) cout << ORANGE << "Constructing Path..." << RESET << endl;
	string	root;
		if (loc->getRoot().empty())
			root = _server_config.getRoot();
		else
			root = loc->getRoot();
	if (D_POST) cout << YELLOW << "Using root: " << root << RESET << endl;
	string uri = _request->getUri();
	if (D_POST) cout << YELLOW << "Using URI: " << uri << RESET << endl;
	
	string path;
	path = root + uri;
	if (D_POST) cout << GREEN << "Resolved path: " << path << RESET << endl;
	// Handle Directory
	// For POST, we don't check if the file exists, but if the upload path (directory) is valid.
	size_t path_separator = path.find_last_of('/');
	if (path_separator != std::string::npos) {
		string	dirPath = path.substr(0, path_separator);
		if (!isDirectory(dirPath)) {
			// The directory doesn't exist. Return a 409 Conflict error.
			if (D_POST) cout << RED << "Upload directory does not exist: " << dirPath << RESET << endl;
			_statusCode = 409;
			_reasonPhrase = generateStatusMessage(_statusCode);
			_responseBody = getErrorPageContent(_statusCode);
			_contentLength = _responseBody.size();
			return;
		}
	}
	// 1. Check the type of content
	const string&	contentType = _request->getHeaderValue("content-type");

	// --- Simple File Upload Logic ---
	// This handles plain text, images, pdfs, etc. - anything to be saved as a file.
	if (contentType.find("text/plain") != string::npos || contentType.find("image/") != string::npos) {
		
		// 3. Write the raw request body to the file
		std::ofstream	file(path.c_str(), std::ios::binary);
		if (!file.is_open()) {
			if(D_POST) cout << RED << "POST. Could not open file for writing: " << path << RESET << endl;
			// Handle error (permission denied, etc.)
			_statusCode = 500;
			_reasonPhrase = generateStatusMessage(_statusCode);
			_responseBody = getErrorPageContent(_statusCode);
			_contentLength = _responseBody.size();
			return;
		}
		file << _request->getBody();
		file.close();

		if(D_POST) cout << GREEN << "POST. File created at: " << path << RESET << endl;
		
		// 4. Send a "Created" response
		_statusCode = 201;
		_reasonPhrase = generateStatusMessage(_statusCode);
		// Set the 'Location' header to the URI of the new file
		// it's good practice to include a Location header that points 
		// to the newly created resource.
		_headers["Location"] = _request->getUri();

		_responseBody = "<html><body><h1>201 Created</h1><p>Resource created at <a href=\"" + _request->getUri() + "\">" + _request->getUri() + "</a></p></body></html>";
		
		_contentLength = _responseBody.size();
		return ;
	} 
	// --- Multipart Form Data Logic (File Upload) ---
	/* else if (contentType.find("multipart/form-data") != string::npos) {
        // Parse multipart data to extract the file
        // You'll need to implement parseMultipartData() method
        string filename, fileData;
        if (parseMultipartData(_request->getBody(), contentType, filename, fileData)) {
            // Construct file path with the original filename
            string uploadPath = path;
            if (uploadPath[uploadPath.length() - 1] != '/') {
                uploadPath += "/";
            }
            uploadPath += filename;
            
            // Write the file data
            std::ofstream file(uploadPath.c_str(), std::ios::binary);
            if (!file.is_open()) {
                if(D_POST) cout << RED << "POST. Could not open file for writing: " << uploadPath << RESET << endl;
                _statusCode = 500;
                _reasonPhrase = generateStatusMessage(_statusCode);
                _responseBody = getErrorPageContent(_statusCode);
                _contentLength = _responseBody.size();
                return;
            }
            file << fileData;
            file.close();

            if(D_POST) cout << GREEN << "POST. File uploaded: " << uploadPath << RESET << endl;
            
            _statusCode = 201;
            _reasonPhrase = generateStatusMessage(_statusCode);
            _headers["Location"] = _request->getUri() + filename;
            _responseBody = "<html><body><h1>201 Created</h1><p>File uploaded successfully: " + filename + "</p></body></html>";
            _contentLength = _responseBody.size();
            return;
        } else {
            _statusCode = 400; // Bad Request
            _reasonPhrase = generateStatusMessage(_statusCode);
            _responseBody = getErrorPageContent(_statusCode);
            _contentLength = _responseBody.size();
            return;
        }
    } */



	else if (contentType.find("application/x-www-form-urlencoded") != string::npos) {
			// --- Form Data Logic ---
			// TODO: Parse the request body to get key-value pairs.
			// For example, parse "name=Maryna&city=Kyiv"
			_statusCode = 501; // Not Implemented yet
			_reasonPhrase = generateStatusMessage(_statusCode);
			_contentLength = _responseBody.size();
	} else {
			// --- Unsupported Type Logic ---
			// The server doesn't know how to handle this content type.
			_statusCode = 415; // Unsupported Media Type
			_reasonPhrase = generateStatusMessage(_statusCode);
			_responseBody = getErrorPageContent(_statusCode);
			_contentLength = _responseBody.size();
		}	
}

// Helper to find the best matching location
// Returns a pointer to the Location object, or NULL if none found
const Location* Response::matchPathToLocation() {
	if (!_request) return NULL;

	const std::vector<Location>&	locations = _server_config.getLocations();
	const Location*					bestMatch = NULL;
	size_t							bestMatchLen = 0;

	if (DEBUG) cout << GREEN << "Matching URI: [" << _request->getUri() << "] against ";
	if (DEBUG) cout << locations.size() << " locations." << RESET << endl;

	for (size_t i = 0; i < locations.size(); ++i) {
		const string& locPath = locations[i].getPath();
		if (DEBUG) cout << GREEN << "  Checking location: [" << locPath << "]" << RESET << endl;
		
		// Check if request URI starts with this location path
		if (_request->getUri() == locPath) {
			if (DEBUG) cout << GREEN << "    -> Match found!" << RESET << endl;
			// We want the longest match (e.g. location /images/ vs location /)
			if (locPath.length() > bestMatchLen) {
				bestMatch = &locations[i];
				bestMatchLen = locPath.length();
				if (DEBUG) cout << GREEN << "    -> New best match (length " << bestMatchLen;
				if (DEBUG) cout << ")" << RESET << endl;
			}
		}
	}
	return bestMatch;
}

// Helper to find the location that is the longest matching prefix of the request URI.
const Location*	Response::matchPrefixPathToLocation() {

	const std::vector<Location>&	locations = _server_config.getLocations();
	const Location*					bestMatch = NULL;
	size_t							bestMatchLen = 0;
	const string&					RequestUri = _request->getUri();

	if (D_POST) cout << GREEN << "Matching URI: [" << RequestUri << "] against ";
	if (D_POST) cout << locations.size() << " locations." << RESET << endl;

	for (size_t i = 0; i < locations.size(); ++i) {

		const string&	locPath = locations[i].getPath();
		if (D_POST) cout << GREEN << "  Checking location: [" << locPath << "]" << RESET << endl;
		
		if (RequestUri.rfind(locPath, 0) == 0) {

			if (D_POST) cout << BLUE << "    Request URI starts with the location path" << RESET << endl;
			if (prefixMatching(locPath, RequestUri)) {
				continue;
			}
			if (D_POST) cout << GREEN << "    -> Prefix Match found!" << RESET << endl;
			// We want the longest match (e.g. location /images/ vs location /)
			if (locPath.length() > bestMatchLen) {
				bestMatch = &locations[i];
				bestMatchLen = locPath.length();
				if (D_POST) cout << GREEN << "    -> New best match (length " << bestMatchLen;
				if (D_POST) cout << ")" << RESET << endl;
			}
		}
	}
	return bestMatch;
}

// It should only match if there's a "/" after the match.
// e.g. URI "/about-us/team.html" should not match location "/about"
// The character at index 6 of "/about-us/team.html" is a hyphen (-).
bool	Response::prefixMatching(const string& locPath, const string& RequestUri) {

	//cout << RED << "RequestUri: " << RequestUri << "; LocPrefixPath: " << locPath << endl;
	// character in the URI right after the prefix
	size_t	index = locPath.length();

	if (locPath.length() > 1 && RequestUri.length() > locPath.length() 
		&& RequestUri[index] != '/') {
			return true;
	} else {
		return false;
	}
}

// returns the index file name for the matched location or an empty string if none found
std::string Response::getIndexFromLocation() {
	for (std::vector<Location>::const_iterator it = _server_config.getLocations().begin(); 
			it != _server_config.getLocations().end(); ++it) {
				if (DEBUG) {
					cout << "Checking location for index: " << it->getPath() << endl;
					cout << ORANGE << "Request URI: " << _request->getUri() << RESET << endl;
					cout << ORANGE << "Location Path: " << it->getPath() << RESET << endl;
					cout << ORANGE << "Location Index: " << it->getIndex() << RESET << endl;
				}
		if (_request->getUri() == it->getPath()) {
			return "/" + it->getIndex();
		}
	}
	return "";
}

void Response::generateResponse() {
	if (!_request) return;

	const Location* loc = matchPrefixPathToLocation();
	//const Location* loc = matchPathToLocation();
	if (!loc) {
		if (DEBUG) cout << RED << "No location matched." << RESET << endl;
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
			if (DEBUG) cout << RED << "Method " << _request->getMethod() << " not allowed for this location." << RESET << endl;
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
		string root = !loc->getRoot().empty() ? loc->getRoot() : _server_config.getRoot();
		if (DEBUG) cout << YELLOW << "Using root: " << root << RESET << endl;
		string uri = _request->getUri();
		if (DEBUG) cout << YELLOW << "Using URI: " << uri << RESET << endl;
		string path;
		path = root + uri + getIndexFromLocation();
		if (DEBUG) cout << GREEN << "Resolved path: " << path << RESET << endl;

		// Handle Directory
		if (isDirectory(path)) {		
			if (DEBUG) cout << BLUE << "Path is a directory." << RESET << endl;
			if (path[path.length() - 1] != '/') path += "/";
			
			// Check for index file
			string indexFile = !loc->getIndex().empty() ? loc->getIndex() : "index.html";
			string indexPath = path + indexFile;
			
			if (DEBUG) cout << BLUE << "Checking index file: " << indexPath << RESET << endl;
			if (DEBUG) cout << YELLOW << "Index to use: " << loc->getIndex() << RESET << endl;

			if (!loc->getIndex().empty()){
				path = indexPath; // Index exists, serve this file
				if (DEBUG) cout << GREEN << "Index file found: " << path << RESET << endl;
			} else {
				if (DEBUG) cout << BLUE << "Index file not found." << RESET << endl;
				// No index file found. Check Autoindex.
				if (loc->getAutoindex()) {
					if (DEBUG) cout << BLUE << "Autoindex is ON. Generating listing for: " << path << RESET << endl;
					string body;
					size_t bodyLen = 0;
					if (buildHtmlIndexTable(path, body, bodyLen) == 0) {
						_statusCode = 200;
						_reasonPhrase = generateStatusMessage(_statusCode);
						_responseBody = body;
						_contentLength = bodyLen;
						return;
					}
					if (DEBUG) cout << RED << "Autoindex generation failed." << RESET << endl;
					_statusCode = 500;
					_reasonPhrase = generateStatusMessage(_statusCode);
					_responseBody = getErrorPageContent(_statusCode);
					_contentLength = _responseBody.size();
					return;
				} else {
					// Directory, no index, no autoindex -> Forbidden
					if (DEBUG) cout << RED << "Directory access forbidden (no index, autoindex off)" << RESET << endl;
					_statusCode = 403;
					_reasonPhrase = generateStatusMessage(_statusCode);
					_responseBody = getErrorPageContent(_statusCode);
					_contentLength = _responseBody.size();
					return;
				}
			}
		}

		// 3. Serve File
		if (DEBUG) cout << BLUE << "Serving file: " << path << RESET << endl;
		std::ifstream file(path.c_str());
		if (!file.is_open()) {
			if (DEBUG) cout << RED << "File not found: " << path << RESET << endl;
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

string Response::getResponseBody() const {
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

string Response::getReasonPhrase() const {
	return _reasonPhrase;
}

const std::map<string, string>& Response::getHeaders() const {
	return _headers;
}

string Response::getErrorPageContent(int code) {
	const std::map<int, string>& errorPages = _server_config.getErrorPages();
	std::map<int, string>::const_iterator it = errorPages.find(code);
	
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