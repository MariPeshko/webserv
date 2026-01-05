#include "Response.hpp"
#include "../httpContext/HttpParser.hpp"
#include "../cgi/CgiHandler.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::string;

// Parametric constructor
Response::Response(Server &server)
	: _server_config(server),
	  _request(0),
	  _statusCode(200),
	  _reasonPhrase(generateStatusMessage(200)),
	  _contentLength(0)
{ }

Response &Response::operator=(const Response &other)
{
	if (this != &other)	{
		_server_config = other._server_config;
		_request = other._request;
		_statusCode = other.getStatusCode();
		_reasonPhrase = other._reasonPhrase;
		_contentLength = other.getContentLength();
	}
	return *this;
}

Response::~Response() {}

// call after parsing
void	Response::bindRequest(const Request &req) {
	_request = &req;
}

void	Response::fillResponse(short statusCode, const string &bodyContent)
{
	_statusCode = statusCode;
	_reasonPhrase = generateStatusMessage(_statusCode);
	_responseBody = bodyContent;
	_contentLength = _responseBody.size();
}

// Method DELETE and generation of the response
// set status code success 204 (No Content).
void	Response::deleteAndGenerateResponse()
{
	const Location*	loc = validateRequestAndGetLocation();
	if (!loc) {
		return; // Response is already filled by the validation function
	}
	
	// Construct Path
	if (D_POST) cout << ORANGE << "Constructing Path..." << RESET << endl;
	string	root;
	if (loc->getRoot().empty())
		root = _server_config.getRoot();
	else
		root = loc->getRoot();
	if (D_POST) cout << YELLOW << "Using root: " << root << RESET << endl;
	string	uri = _request->getUri();
	if (D_POST) cout << YELLOW << "Using URI: " << uri << RESET << endl;

	string	path;
	path = root + uri;
	if (D_POST) cout << GREEN << "Resolved path: " << path << RESET << endl;
	
	// Check if the file/resource exists
	PathType pathType = getPathType(path);
	if (pathType == NOT_EXIST) {
		if (D_POST) cout << RED << "Resource not found: " << path << RESET << endl;
		fillResponse(404, getErrorPageContent(404));
		return;
	}
	// Don't allow deleting directories (optional, depends on your requirements)
	if (pathType == DIRECTORY_PATH) {
		if (D_POST) cout << RED << "Cannot delete directory: " << path << RESET << endl;
		fillResponse(403, getErrorPageContent(403)); // Forbidden
		return;
	}
	// Attempt to delete the file
	if (std::remove(path.c_str()) != 0) { // Deletion failed (permission denied, etc.)
		if (D_POST) cout << RED << "Failed to delete file: " << path << RESET << endl;
		fillResponse(403, getErrorPageContent(403)); // Forbidden
		return;
	}
	if (D_POST) cout << GREEN << "File deleted successfully: " << path << RESET << endl;
	// Success: 204 No Content
	fillResponse(204, "");
}

// Method POST and generation of the response
// To Do refactor code that ised in generateResponse():
// 1. matchPrefixPathToLocation()(); 2. allowed = loc->getAllowedMethods();
// 3. if (!allowed.empty()) 4. Check for redirection if (loc->getReturnCode() != 0)
void	Response::postAndGenerateResponse()
{
	const Location*	loc = validateRequestAndGetLocation();
	if (!loc) {
		return; // Response is already filled by the validation function
	}

	// Construct Path
	if (D_POST) cout << ORANGE << "Constructing Path..." << RESET << endl;
	string	root;
	if (loc->getRoot().empty())
		root = _server_config.getRoot();
	else
		root = loc->getRoot();
	if (D_POST) cout << YELLOW << "Using root: " << root << RESET << endl;
	string	uri = _request->getUri();
	if (D_POST) cout << YELLOW << "Using URI: " << uri << RESET << endl;

	string	path;
	path = root + uri;
	if (D_POST) cout << GREEN << "Resolved path: " << path << RESET << endl;

	// Check for CGI first
	const std::map<std::string, std::string>& cgiMap = loc->getCgi();
	if (!cgiMap.empty()) {
		size_t dotPos = path.find_last_of('.');
		if (dotPos != string::npos) {
			string ext = path.substr(dotPos + 1);
			std::map<std::string, std::string>::const_iterator it = cgiMap.find(ext);
			if (it != cgiMap.end()) {
				// For POST, the script must exist
				if (getPathType(path) == FILE_PATH) {
					if (DEBUG) cout << GREEN << "Executing CGI (POST): " << path << RESET << endl;
					try {
						CgiHandler cgi(*_request, path, it->second);
						string output = cgi.executeCgi();
						
						// Separate Headers and Body from CGI output
						size_t headerEnd = output.find("\r\n\r\n");
						if (headerEnd == string::npos) {
							headerEnd = output.find("\n\n");
						}

						if (headerEnd != string::npos) {
							string headers = output.substr(0, headerEnd);
							string body = output.substr(headerEnd + ((output[headerEnd] == '\r') ? 4 : 2));

							// Simple parsing of headers
							size_t start = 0;
							size_t end = headers.find('\n');
							while (end != string::npos) {
								string line = headers.substr(start, end - start);
								if (!line.empty() && line[line.length() - 1] == '\r') line.erase(line.length() - 1);
								size_t colon = line.find(':');
								if (colon != string::npos) {
									string key = line.substr(0, colon);
									string value = line.substr(colon + 1);
									while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) value.erase(0, 1);
									_headers[key] = value;
								}
								start = end + 1;
								end = headers.find('\n', start);
							}
							if (start < headers.length()) {
								string line = headers.substr(start);
								if (!line.empty() && line[line.length() - 1] == '\r') line.erase(line.length() - 1);
								size_t colon = line.find(':');
								if (colon != string::npos) {
									string key = line.substr(0, colon);
									string value = line.substr(colon + 1);
									while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) value.erase(0, 1);
									_headers[key] = value;
								}
							}
							fillResponse(200, body);
						} else {
							fillResponse(200, output);
						}
						return;
					} catch (std::exception &e) {
						if (DEBUG) cout << RED << "CGI execution failed: " << e.what() << RESET << endl;
						fillResponse(500, getErrorPageContent(500));
						return;
					}
				}
			}
		}
	}

	// For POST, we don't check if the file exists, but if the upload path (directory) is valid.
	// Handle Directory
	size_t	path_separator = path.find_last_of('/');
	if (path_separator != string::npos) {
		string	dirPath = path.substr(0, path_separator);
		if (!isDirectory(dirPath)) {
			// The directory doesn't exist. Return a 409 Conflict error.
			if (D_POST) cout << RED << "Upload directory does not exist: " << dirPath << RESET << endl;
			fillResponse(409, getErrorPageContent(409));
			return;
		}
	}
	// 1. Check the type of content
	const string&	contentType = _request->getHeaderValue("content-type");
	// --- Simple File Upload Logic ---
	// This handles plain text, images, pdfs, etc. - anything to be saved as a file.
	if (contentType.find("text/plain") != string::npos || contentType.find("image/") != string::npos)
	{
		// 3. Write the raw request body to the file
		std::ofstream	file(path.c_str(), std::ios::binary);
		if (!file.is_open()) {
			if (D_POST) cout << RED << "POST. Could not open file for writing: " << path << RESET << endl;
			// Handle error (permission denied, etc.)
			fillResponse(500, getErrorPageContent(500));
			return;
		}
		file << _request->getBody();
		file.close();

		if (D_POST) cout << GREEN << "POST. File created at: " << path << RESET << endl;

		// 4. Send a "Created" response
		fillResponse(201,
			"<html><body><h1>201 Created</h1><p>Resource created at <a href=\"" + _request->getUri() + "\">" + _request->getUri() + "</a></p></body></html>");
		// Set the 'Location' header to the URI of the new file
		// it's good practice to include a Location header that points
		// to the newly created resource.
		_headers["Location"] = _request->getUri();
		_headers["Content-Type"] = "text/html";
		return;
	}
	// --- Multipart Form Data Logic (File Upload) ---
	else if (contentType.find("multipart/form-data") != string::npos) {
		// Parse multipart data to extract the file
		string	filename, fileData;

		string	boundary = HttpParser::extractBoundary(_request->getHeaderValue("content-type"));
		if (HttpParser::parseMultipartData(_request->getBody(), boundary, filename, fileData))
		{
			if (!HttpParser::isExtensionAllowed(filename)) {
				fillResponse(415, getErrorPageContent(415));
				return;
			}
			// Construct file path with the original filename
			string	uploadPath = path;
			uploadPath += filename;

			if (D_POST) cout << ORANGE << "uploadPath: " << uploadPath << RESET << endl;
			// Write the file data
			std::ofstream	file(uploadPath.c_str(), std::ios::binary);
			if (!file.is_open()) {
				if (D_POST) cout << RED << "POST. Could not open file for writing: " << uploadPath << RESET << endl;
				fillResponse(500, getErrorPageContent(500));
				return;
			}
			file << fileData;
			file.close();

			if (D_POST) cout << GREEN << "POST. File uploaded. Post/Redirect/Get (PRG) pattern: " << uploadPath << RESET << endl;

			// go back to the form page (the Referer)
			string	redirectTo;
			string	referer = _request->getHeaderValue("referer");
			if (!referer.empty()) { 		// extract of path from "http://host/path?..."
				size_t	pos = referer.find("://");
				if (pos != string::npos) {
					pos = referer.find('/', pos + 3);
				} else {
					pos = referer.find('/');
				}
				if (pos != string::npos) {
					redirectTo = referer.substr(pos);
					if (D_POST) cout << GREEN << "Redirect to the Referer: " << redirectTo << RESET << endl;
				}
			}
			if (redirectTo.empty()) {
				fillResponse(201, "<html><body><h1>201 Created</h1><p>File uploaded successfully: " +
					filename + "</p></body></html>");
			_headers["Location"] = _request->getUri() + filename;
				_headers["Content-Type"] = "text/html";
			}
			// Instead of 201, send a 303 redirect back to the uploads page
			fillResponse(303, ""); // 303 See Other, empty body
			_headers["Location"] = redirectTo;
			// TO DELETE _headers["Location"] = "/uploads/uploads.html"; // Redirect back to the form
			return;
		} else {
			fillResponse(400, getErrorPageContent(400));
			return;
		}
	} else if (contentType.find("application/x-www-form-urlencoded") != string::npos) {
		// --- Form Data Logic ---
		// TODO: Parse the request body to get key-value pairs.
		// For example, parse "name=Maryna&city=Kyiv"
		fillResponse(501, getErrorPageContent(501)); // Not Implemented yet
	} else {
		// --- Unsupported Type Logic ---
		// The server doesn't know how to handle this content type.
		fillResponse(415, getErrorPageContent(415));
	}
}

// Debugging purpose for matchPathToLocation()
static void	printCurrentLocation(const Location *loc) {
	if (loc) {
		cout << "Current matched location: " << loc->getPath() << endl;
		cout << "  Root: " << loc->getRoot() << endl;
		cout << "  Index: " << loc->getIndex() << endl;
		cout << "  Autoindex: " << (loc->getAutoindex() ? "on" : "off") << endl;
	} else {
		cout << "No location matched." << endl;
	}
}

/**
 * Helper to find the best matching location
 * 
 * Checks if request URI starts with a certain location path (proper prefix match)
 * Returns a pointer to the Location object, or NULL if none found
 * Special case: "/" matches everything starting with "/"
 * Looks for the longest match (location /images/ vs location /)
 */
const Location*	Response::matchPathToLocation()
{
	if (!_request) return NULL;

	const std::vector<Location>&	locations = _server_config.getLocations();
	const Location*					bestMatch = NULL;
	size_t							bestMatchLen = 0;

	if (DEBUG_PATH) cout << GREEN << "Matching URI: [" << _request->getUri() << "] against ";
	if (DEBUG_PATH) cout << locations.size() << " locations." << RESET << endl;

	for (size_t i = 0; i < locations.size(); ++i) {
		const string&	locPath = locations[i].getPath();
		const string&	uri = _request->getUri();
		const string	html_ext = ".html";
		
		if (DEBUG_PATH) cout << GREEN << "  Checking location: [" << locPath << "]" << RESET << endl;
		if (DEBUG_PATH) cout << ORANGE << "    Comparing URI: " << uri << " with Location Path: " << locPath << RESET << endl;

		if (uri.compare(0, locPath.length(), locPath) == 0)
		{
			bool	isValidPrefix = false;

			if (locPath == "/") { // Root location matches all URIs starting with "/"
				isValidPrefix = true;
			} else if (uri.length() == locPath.length()) { // Exact match (/about matches /about)
				isValidPrefix = true;
			} else if (uri[locPath.length()] == '/') { // Path continues with '/' (/about matches /about/page)
				isValidPrefix = true;
			}
			if (isValidPrefix) {
				if (DEBUG_PATH) cout << GREEN << "    -> Match found!" << RESET << endl;
				if (locPath.length() > bestMatchLen) {
					bestMatch = &locations[i];
					bestMatchLen = locPath.length();
					if (DEBUG_PATH) {
						cout << GREEN << "    -> New best match (length " << bestMatchLen << ")" << RESET << endl;
						cout << GREEN << "    -> Best match root: " << bestMatch->getRoot() << RESET << endl;
					}
				}
			} else {
				if (DEBUG_PATH) cout << YELLOW << "    -> Substring match but not path prefix (rejected)" << RESET << endl;
			}
		}
	}
	if (DEBUG_PATH) printCurrentLocation(bestMatch);
	return bestMatch;
}

// returns the index file name for the matched location or an empty string if none found
string	Response::getIndexFromLocation()
{
	for (std::vector<Location>::const_iterator it = _server_config.getLocations().begin();
		 it != _server_config.getLocations().end(); ++it)
	{
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

Response::PathType Response::getPathType(string const path)
{
	struct stat	buffer;
	int			result;

	result = stat(path.c_str(), &buffer);

	if (result == 0)
	{
		if (buffer.st_mode & S_IFDIR)
			return (DIRECTORY_PATH);
		else if (buffer.st_mode & S_IFREG)
			return (FILE_PATH);
		else
			return (NOT_EXIST);
	}
	return (NOT_EXIST);
}

void	Response::badRequest() {
	if (DEBUG) cout << RED << "Response. Bad request" << RESET << endl; 
	if (_request->getRequestLineFormatValid() == false) {
		fillResponse(400, getErrorPageContent(400));
	} else if (_request->getHeadersFormatValid() == false) {
		if (DEBUG) cout << RED << "Response. Bad request. Invalid headers" << RESET << endl; 
		fillResponse(400, getErrorPageContent(400));
	} else {
		fillResponse(400, getErrorPageContent(400));
	}
}

void	Response::generateResponse()
{
	if (!_request) return;
	fillResponse(200, "");

	const Location*	loc = validateRequestAndGetLocation();
	if (!loc) {
		return; // Response is already filled by the validation function
	}

	// Construct Path
	string			root;
	const string&	locationRoot = loc->getRoot();
	if (!locationRoot.empty())
		root = loc->getRoot();				// Use location-specific root
	else 
		root = _server_config.getRoot();	// Fall back to server root
	// Safety. Fallback if root is empty: use current directory.
	if (root.empty()) root = "."; // the current working directory
	
	string			uri = _request->getUri();
	string			path = root + uri;
	
	if (DEBUG) cout << YELLOW << "Using root: " << root << RESET << endl;
	if (DEBUG) cout << YELLOW << "Using URI: " << uri << RESET << endl;
	if (DEBUG) cout << GREEN << "Resolved path: " << path << RESET << endl;

	// Check if path exists
	PathType	pathType = getPathType(path);
	// CRITICAL: If location exists in config but path doesn't exist on disk → 404
	if (pathType == NOT_EXIST) {
		if (DEBUG) cout << RED << "Path not found: " << path << RESET << endl;
		fillResponse(404, getErrorPageContent(404));
		return;
	}
	// Handle Directory
	if (pathType == DIRECTORY_PATH) {
		if (DEBUG) cout << BLUE << "Path is a directory.1" << RESET << endl;
		if (path[path.length() - 1] != '/')
			path += "/";
		// if autoindex true
		//
		bool	isIndexPresent = loc->getIndex().empty() ? false : true;
		bool	isAutoindexPresent = loc->getAutoindex() ? true : false;
		// Check for index file
		string	indexFile = isIndexPresent ? loc->getIndex() : "";
		// string targetPath = path + indexFile;
		string	targetPath = indexFile.empty() ? isAutoindexPresent ? path : "" : path + indexFile;

		if (DEBUG) cout << BLUE << "Checking targetPath file1: " << targetPath << RESET << endl;
		if (DEBUG) cout << YELLOW << "Index to use: " << indexFile << RESET << endl;

		if (getPathType(targetPath) == FILE_PATH) {
			path = targetPath; // Index exists, serve this file
			if (DEBUG) cout << GREEN << "Index file exists1: " << path << RESET << endl;
		} else {
			if (DEBUG) cout << BLUE << "Index file not found." << RESET << endl;
			// No index file found. Check Autoindex.
			if (loc->getAutoindex()) {
				if (DEBUG) cout << BLUE << "Autoindex is ON. Generating listing for: " << path << RESET << endl;
				string	body;
				size_t	bodyLen = 0;
				if (buildHtmlIndexTable(path, body, bodyLen) == 0) {
					if (DEBUG) cout << "buildHtmlIndexTable(). " << "path: " << path << endl;
					_headers["Content-Type"] = "text/html";
					fillResponse(200, body);
					return;
				}
				if (DEBUG) cout << RED << "Autoindex generation failed." << RESET << endl;
				fillResponse(500, getErrorPageContent(500));
				return;
			} else {
				// Directory, no index, no autoindex -> Forbidden
				if (DEBUG) cout << RED << "Directory access forbidden (no index, autoindex off2)" << RESET << endl;
				fillResponse(403, getErrorPageContent(403));
				return;
			}
		}
	}
	// 3. Serve File
	// Check for CGI
	const std::map<std::string, std::string>& cgiMap = loc->getCgi();
	if (!cgiMap.empty()) {
		size_t dotPos = path.find_last_of('.');
		if (dotPos != string::npos) {
			string ext = path.substr(dotPos + 1);
			std::map<std::string, std::string>::const_iterator it = cgiMap.find(ext);
			if (it != cgiMap.end()) {
				if (DEBUG) cout << GREEN << "Executing CGI: " << path << RESET << endl;
				try {
					CgiHandler cgi(*_request, path, it->second);
					string output = cgi.executeCgi();
					
					// Separate Headers and Body from CGI output
					size_t headerEnd = output.find("\r\n\r\n");
					if (headerEnd == string::npos) {
						headerEnd = output.find("\n\n");
					}

					if (headerEnd != string::npos) {
						string headers = output.substr(0, headerEnd);
						string body = output.substr(headerEnd + ((output[headerEnd] == '\r') ? 4 : 2));

						// Simple parsing of headers (can be improved)
						size_t start = 0;
						size_t end = headers.find('\n');
						while (end != string::npos) {
							string line = headers.substr(start, end - start);
							// Remove \r if present
							if (!line.empty() && line[line.length() - 1] == '\r') {
								line.erase(line.length() - 1);
							}
							size_t colon = line.find(':');
							if (colon != string::npos) {
								string key = line.substr(0, colon);
								string value = line.substr(colon + 1);
								// Trim whitespace
								while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) value.erase(0, 1);
								_headers[key] = value;
							}
							start = end + 1;
							end = headers.find('\n', start);
						}
						// Process last line
						if (start < headers.length()) {
							string line = headers.substr(start);
							if (!line.empty() && line[line.length() - 1] == '\r') line.erase(line.length() - 1);
							size_t colon = line.find(':');
							if (colon != string::npos) {
								string key = line.substr(0, colon);
								string value = line.substr(colon + 1);
								while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) value.erase(0, 1);
								_headers[key] = value;
							}
						}
						
						fillResponse(200, body);
					} else {
						// No headers found, treat entire output as body
						fillResponse(200, output);
					}
					return;
				} catch (std::exception &e) {
					if (DEBUG) cout << RED << "CGI execution failed: " << e.what() << RESET << endl;
					fillResponse(500, getErrorPageContent(500));
					return;
				}
			}
		}
	}

	if (DEBUG) cout << BLUE << "Serving file: " << path << RESET << endl;
	std::ifstream	file(path.c_str());
	if (!file.is_open()) {
		if (DEBUG) cout << RED << "File not found: " << path << RESET << endl;
		fillResponse(404, getErrorPageContent(404));
		return;
	}

	string	contentType = getMimeType(path);
	_headers["Content-Type"] = contentType;

	std::ostringstream	ss;
	ss << file.rdbuf();
	_responseBody = ss.str();
	_contentLength = _responseBody.size();
	file.close();
}

/**
 * @brief Performs common request validation checks.
 * 
 * This function validates the request against the server configuration.
 * It checks for:
 * 1. A matching location.
 * 2. If the request method is allowed in that location.
 * 3. If the location has a redirection configured.
 * 
 * If any of these checks result in a final response (404, 405, 3xx),
 * it fills the response and returns NULL.
 * 
 * @return A const pointer to the matched Location on success, 
 * or NULL on failure.
 */
const Location*	Response::validateRequestAndGetLocation() {
	const Location*	loc = matchPathToLocation();
	if (!loc) {
		if (DEBUG) cout << RED << "Resource not found: " << _request->getUri() << RESET << endl;
		fillResponse(404, getErrorPageContent(404));
		return NULL;
	}
	// Check for allowed methods
	const std::vector<string>&	allowed = loc->getAllowedMethods();
	if (!allowed.empty()) {
		bool	methodAllowed = false;
		for (size_t i = 0; i < allowed.size(); ++i) {
			if (allowed[i] == _request->getMethod()) {
				methodAllowed = true;
				break;
			}
		}
		if (!methodAllowed) {
			if (DEBUG) cout << RED << "Method " << _request->getMethod() << " not allowed for this location." << RESET << endl;
			fillResponse(405, getErrorPageContent(405));
			return NULL;
		}
	}
	// Check for redirection
	int	code = loc->getReturnCode();
	if (code != 0) {
		fillResponse(code, "<html><body><h1>" + toString(code) + " " +
						generateStatusMessage(code) + "</h1></body></html>");
		_headers["Location"] = loc->getReturnUrl();
		_headers["Content-Type"] = "text/html";
		return NULL;
	}
	return loc;
}

short			Response::getStatusCode() const { return _statusCode; }

size_t			Response::getContentLength() const { return _contentLength; }

const string&	Response::getResponseBody() const {
	return _responseBody;
}

const string&	Response::getReasonPhrase() const {
	return _reasonPhrase;
}

Server&			Response::getServerConfig() {
	return _server_config;
}

const std::map<string, string>&	Response::getHeaders() const {
	return _headers;
}

/** nulls the Request* before new HTTP request-response cycle */
void			Response::reset()
{
	_request = 0;
	fillResponse(200, "");
	_headers.clear();
}

string			Response::getErrorPageContent(int code)
{
	const std::map<int, string>&			errorPages = _server_config.getErrorPages();
	std::map<int, string>::const_iterator	it = errorPages.find(code);

	if (it != errorPages.end()) {
		std::ifstream	file(it->second.c_str());
		if (file.is_open())
		{
			std::ostringstream	ss;
			
			ss << file.rdbuf();
			_headers["Content-Type"] = "text/html";
			return ss.str();
		}
	}
	return "";
}

string			Response::getMimeType(const string &filePath)
{
	size_t	dotPos = filePath.find_last_of('.');
	if (dotPos == string::npos) {
		return "application/octet-stream";
	}

	string extension = filePath.substr(dotPos);

	// Convert to lowercase
	for (size_t i = 0; i < extension.length(); ++i) {
		extension[i] = std::tolower(extension[i]);
	}

	if (extension == ".jpg" || extension == ".jpeg") {
		return "image/jpeg";
	} else if (extension == ".png") {
		return "image/png";
	} else if (extension == ".gif") {
		return "image/gif";
	} else if (extension == ".ico") {
		return "image/x-icon";
	} else if (extension == ".html" || extension == ".htm") {
		return "text/html";
	} else if (extension == ".css") {
		return "text/css";
	} else if (extension == ".js") {
		return "application/javascript";
	} else if (extension == ".txt") {
		return "text/plain";
	} else if (extension == ".svg") {
		return "image/svg+xml";
	}
	return "application/octet-stream";
}
