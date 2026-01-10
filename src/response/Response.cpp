#include "Response.hpp"

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
	  _contentLength(0),
	  _loc(0)
{ }

Response &Response::operator=(const Response &other) {
	if (this != &other)	{
		_server_config = other._server_config;
		_request = other._request;
		_statusCode = other.getStatusCode();
		_reasonPhrase = other._reasonPhrase;
		_contentLength = other.getContentLength();
		_path = other._path;
		_loc = other._loc;
	}
	return *this;
}

Response::~Response() {}

// call after parsing
void	Response::bindRequest(const Request &req) {	_request = &req; }

void	Response::fillResponse(short statusCode, const string &bodyContent)
{
	_statusCode = statusCode;
	_reasonPhrase = generateStatusMessage(_statusCode);
	_responseBody = bodyContent;
	_contentLength = _responseBody.size();
}

/**
 * @brief Main entry point for generating HTTP responses.
 * 
 * Validates the request, constructs the file system path, and dispatches
 * to the appropriate method handler (GET, POST, DELETE) based on the HTTP method.
 * Returns early if validation or path construction fails (response already set).
 */
void	Response::generateResponse() {
	if (!getRequest()) return;

	_loc = validateRequestAndGetLocation();
	if (!_loc) {
		return; // Response is already filled by the validation function
	}

	_path = constructPath(_loc);
	if (_path.empty())
		return;
	
	if (getRequest()->getEnumMethod() == Request::GET) {
		generateResponseGet();
	} else if (getRequest()->getEnumMethod() == Request::POST) {
		generateResponsePost();
	} else if (getRequest()->getEnumMethod() == Request::DELETE) {
		generateResponseDelete();
	} else if (getRequest()->getEnumMethod() == Request::INVALID) {
		fillResponse(400, getErrorPageContent(400));
	} else {
		fillResponse(400, getErrorPageContent(400));
	}
}

/**
 * @brief Generates HTTP response for GET requests.
 * 
 * Handles three types of resources:
 * - **Files**: Reads and serves with correct MIME type
 * - **Directories**: Serves index file if present, generates autoindex if enabled,
 *   otherwise returns 403 Forbidden
 * - **CGI scripts**: Executes script and returns dynamic output
 * 
 * Returns 404 if the path doesn't exist on disk.
 */
void	Response::generateResponseGet()
{
	PathType	pathType = getPathType(_path);
	if (pathType == NOT_EXIST) {
		if (DEBUG) cout << RED << "Path not found: " << _path << RESET << endl;
		fillResponse(404, getErrorPageContent(404));
		return;
	}
	if (pathType == DIRECTORY_PATH) {
		if (DEBUG) cout << BLUE << "Path is a directory" << RESET << endl;
		if (_path[_path.length() - 1] != '/')
			_path += "/";
		// if autoindex true
		bool	isIndexPresent = _loc->getIndex().empty() ? false : true;
		bool	isAutoindexPresent = _loc->getAutoindex() ? true : false;
		// Check for index file
		string	indexFile = isIndexPresent ? _loc->getIndex() : "";
		// string targetPath = path + indexFile;
		string	targetPath = indexFile.empty() ? isAutoindexPresent ? _path : "" : _path + indexFile;

		if (DEBUG) cout << BLUE << "Checking target Path file: " << targetPath << RESET << endl;
		if (DEBUG) cout << YELLOW << "Index to use: " << indexFile << RESET << endl;

		if (getPathType(targetPath) == FILE_PATH) {
			_path = targetPath; // Index exists, serve this file
			if (DEBUG) cout << GREEN << "Index file exists: " << _path << RESET << endl;
		} else {
			if (DEBUG) cout << BLUE << "Index file not found." << RESET << endl;
			if (isIndexPresent) {
				if (DEBUG) cout << BLUE << "Index is present, but file not found: 404" << RESET << endl;
				fillResponse(404, getErrorPageContent(404));
				return;
			}
			if (_loc->getAutoindex()) {
				if (DEBUG) cout << BLUE << "Autoindex is ON. Generating listing for: " << _path << RESET << endl;
				string	body;
				size_t	bodyLen = 0;
				if (buildHtmlIndexTable(_path, body, bodyLen) == 0) {
					if (DEBUG) cout << "buildHtmlIndexTable(). " << "path: " << _path << endl;
					_headers["Content-Type"] = "text/html";
					fillResponse(200, body);
					return;
				}
				if (DEBUG) cout << RED << "Autoindex generation failed." << RESET << endl;
				fillResponse(500, getErrorPageContent(500));
				return;
			}
			// TO DO
/* 			if (DEBUG) cout << RED << "Directory access forbidden (no index, autoindex off)" << RESET << endl;
			fillResponse(403, getErrorPageContent(403));
			return; */
			// No index and autoindex off then 404 (not 403)
        	fillResponse(404, getErrorPageContent(404));
        	return;
		}
	}
	if (tryServeCgi())
		return;

	if (DEBUG) cout << BLUE << "Serving file: " << _path << RESET << endl;
	std::ifstream	file(_path.c_str());
	if (!file.is_open()) {
		if (DEBUG) cout << RED << "File not found: " << _path << RESET << endl;
		fillResponse(404, getErrorPageContent(404));
		return;
	}

	string	contentType = getMimeType(_path);
	_headers["Content-Type"] = contentType;

	std::ostringstream	ss;
	ss << file.rdbuf();
	_responseBody = ss.str();
	_contentLength = _responseBody.size();
	file.close();
}

string	Response::buildCreatedResponse(const string& uri, const string &filename) {
	string	body;

	body = "<html><body>"
		   "<h1>201 Created</h1>"
		   "<p>Resource created at <a href=\"" + uri + "\">"
		   + uri + "</a></p>";
	if (!filename.empty())
		body = body + "<p>File uploaded successfully: " + filename;
	body = body + "</body></html>";
	return body;
}

/**
 * @brief Generates HTTP response for POST requests - handles file uploads and form submissions.
 * 
 * Logic flow:
 * 1. Tries CGI execution first (for form processing scripts)
 * 2. Validates parent directory exists → 409 if missing
 * 3. Handles three content types:
 *    - text/plain or image/: Direct file write → 201 Created
 *    - multipart/form-data: Parse, extract file, write → 303 redirect to referer
 *    - application/x-www-form-urlencoded: → 501 Not Implemented
 * 4. Returns 415 for unsupported content types
 */
void	Response::generateResponsePost()
{	
	if (tryServeCgi())
		return;
	if (D_POST) cout << BLUE << "generateResponsePost()" << RESET << endl;

	PathType	pathType = getPathType(_path);

	if (pathType == NOT_EXIST) {
		if (DEBUG) cout << RED << "Path not found: " << _path << RESET << endl;
		fillResponse(404, getErrorPageContent(404));
		return;
	}

	// nginx treats POST to a static location as:
	// “I received a body, but I don’t know what to do with it.”

	// here I should check if _path is directory before content parsing
	if (pathType == DIRECTORY_PATH) {
		// consider that it can be without / in the end - www/web/about
		if (D_POST) cout << BLUE << _path << " is a directory" << RESET << endl;

		// 1. Якщо є index файл, і ми хочемо його "обробити"
        // (Але зазвичай POST на індекс без CGI повертає 405)

		// Якщо тіло порожнє: Просто повернути about.html, 
		// ніби це GET, але зі статусом 200 (або 204).
		if (getRequest()->getBody().empty()) {
        	fillResponse(200, "OK");
			return;
    	} else {
			fillResponse(200, "OK");
			return;
		}

		// 2. Якщо ми хочемо дозволити завантаження файлів у цю директорію, 
        // нам потрібне ім'я файлу. В Multipart воно є в заголовках, 
        // а в text/plain — його НЕМАЄ в URI.
		const string&	contentType = getRequest()->getHeaderValue("content-type");
		if (contentType.find("text/plain") != string::npos) {
            // Без імені файлу в URI ми не знаємо, як його назвати.
            // Можна генерувати ім'я (наприклад, "upload_123"), 
            // але зазвичай це 400 Bad Request або 409 Conflict.
            fillResponse(400, getErrorPageContent(400));
            return;
        }
	} else if (pathType == FILE_PATH) {
		if (D_POST) cout << BLUE << _path << " is a file" << RESET << endl;
	}

	// In this case - curl -X POST http://localhost:8080/about --data-binary @/dev/null -
	// there is no name of the file
	size_t	path_separator = _path.find_last_of('/');
	if (path_separator != string::npos) {
		string	dirPath = _path.substr(0, path_separator);
		if (!isDirectory(dirPath)) {
			if (D_POST) cout << RED << "Upload directory does not exist: " << dirPath << RESET << endl;
			fillResponse(409, getErrorPageContent(409));
			return;
		}
	}

	// Is it valid? This case has no contentType
	// curl -X POST http://localhost:8080/about --data-binary @/dev/null
	const string&	contentType = getRequest()->getHeaderValue("content-type");
	if (contentType.empty()) {
		fillResponse(400, getErrorPageContent(400));
		return;
	}
	
	// --- Simple File Upload Logic --- plain text, image - png, jpeg.
	if (contentType.find("text/plain") != string::npos || contentType.find("image/png") != string::npos
			|| contentType.find("image/jpeg") != string::npos)	{
		// Write the raw request body to the file
		std::ofstream	file(_path.c_str(), std::ios::binary);
		if (!file.is_open()) {
			if (D_POST) cout << RED << "Could not open file for writing: " << _path << RESET << endl;
			fillResponse(500, getErrorPageContent(500));
			return;
		}
		file << getRequest()->getBody();
		file.close();

		if (D_POST) cout << GREEN << "File created at: " << _path << RESET << endl;

		// Send a "Created" response
		fillResponse(201, buildCreatedResponse(getRequest()->getUri(), ""));
		_headers["Location"] = getRequest()->getUri();
		_headers["Content-Type"] = "text/html";
		return;
	}
	// --- Multipart Form Data Logic (File Upload) ---
	else if (contentType.find("multipart/form-data") != string::npos) {
		// Parse multipart data to extract the file
		string	filename, fileData;

		string	boundary = HttpParser::extractBoundary(getRequest()->getHeaderValue("content-type"));
		if (HttpParser::parseMultipartData(getRequest()->getBody(), boundary, filename, fileData))
		{
			if (!HttpParser::isExtensionAllowed(filename)) {
				fillResponse(415, getErrorPageContent(415));
				return;
			}
			string	uploadPath = _path;
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
			if (D_POST) cout << GREEN << "POST. File uploaded. Post/Redirect/Get (PRG) pattern: ";
			if (D_POST) cout << uploadPath << RESET << endl;

			// Go back to the form page (the Referer)
			string	redirectTo;
			string	referer = getRequest()->getHeaderValue("referer");
			if (!referer.empty()) { 		// extract of path from "http://host/path?..."
				size_t	pos = referer.find("://");
				if (pos != string::npos)
					pos = referer.find('/', pos + 3);
				else
					pos = referer.find('/');
				if (pos != string::npos) {
					redirectTo = referer.substr(pos);
					if (D_POST) cout << GREEN << "Redirect to the Referer: " << redirectTo << RESET << endl;
				}
			}
			if (redirectTo.empty()) {
				fillResponse(201, buildCreatedResponse(getRequest()->getUri(), filename));
				_headers["Location"] = getRequest()->getUri() + filename;
				_headers["Content-Type"] = "text/html";
			}
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
		// For example, parse "name=Maryna&city=Kyiv"
		if (getRequest()->getBody().empty()) {
        	fillResponse(200, "OK");
    	} else {
			fillResponse(501, getErrorPageContent(501)); // Not Implemented yet
		}
	} else {
		if (getRequest()->getBody().empty()) {
        	fillResponse(200, "OK");
    	} 
		// --- Unsupported Type Logic ---
		fillResponse(415, getErrorPageContent(415));
	}
}

// Method DELETE and generation of the response
// set status code success 204 (No Content).
void	Response::generateResponseDelete()
{
	// DELETE logic
	// Check if the file/resource exists
	PathType pathType = getPathType(_path);
	if (pathType == NOT_EXIST) {
		if (D_POST) cout << RED << "Resource not found: " << _path << RESET << endl;
		fillResponse(404, getErrorPageContent(404));
		return;
	}
	// Don't allow deleting directories (optional, depends on your requirements)
	if (pathType == DIRECTORY_PATH) {
		if (D_POST) cout << RED << "Cannot delete directory: " << _path << RESET << endl;
		fillResponse(403, getErrorPageContent(403)); // Forbidden
		return;
	}

	// Attempt to delete the file
	if (std::remove(_path.c_str()) != 0) { // Deletion failed (permission denied, etc.)
		if (D_POST) cout << RED << "Failed to delete file: " << _path << RESET << endl;
		fillResponse(403, getErrorPageContent(403)); // Forbidden
		return;
	}
	if (D_POST) cout << GREEN << "File deleted successfully: " << _path << RESET << endl;
	// Success: 204 No Content
	fillResponse(204, "");
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
	if (!getRequest()) return NULL;

	const std::vector<Location>&	locations = _server_config.getLocations();
	const Location*					bestMatch = NULL;
	size_t							bestMatchLen = 0;

	const string&					uri = getRequest()->getUri();

	if (DEBUG_PATH) cout << GREEN << "Matching URI: [" << getRequest()->getUri() << "] against ";
	if (DEBUG_PATH) cout << locations.size() << " locations." << RESET << endl;
	
	for (size_t i = 0; i < locations.size(); ++i) {
		const string&	locPath = locations[i].getPath();
		// TO DO: do we need this variable html_ext? 
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
			cout << ORANGE << "Request URI: " << getRequest()->getUri() << RESET << endl;
			cout << ORANGE << "Location Path: " << it->getPath() << RESET << endl;
			cout << ORANGE << "Location Index: " << it->getIndex() << RESET << endl;
		}
		if (getRequest()->getUri() == it->getPath()) {
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
	
	// Check if a specific status code was set during request parsing
	short requestStatusCode = getRequest()->getStatusCode();
	if (requestStatusCode == 400) {
		if (DEBUG) cout << RED << "Response. Bad Request" << RESET << endl;
		fillResponse(400, getErrorPageContent(400));
	} else if (requestStatusCode == 405) {
		if (DEBUG) cout << RED << "Response. Not Allowed" << RESET << endl;
		fillResponse(405, getErrorPageContent(405));
	} else if (requestStatusCode == 413) {
		if (DEBUG) cout << RED << "Response. Payload Too Large" << RESET << endl;
		fillResponse(413, getErrorPageContent(413));
	} else if (requestStatusCode == 414) {
		if (DEBUG) cout << RED << "Response. URI Too Long" << RESET << endl;
		fillResponse(414, getErrorPageContent(414));
	} else if (requestStatusCode == 431) {
		if (DEBUG) cout << RED << "Response. Request Header Fields Too Large" << RESET << endl;
		fillResponse(431, getErrorPageContent(431));
	} else if (getRequest()->getRequestLineFormatValid() == false) {
		fillResponse(400, getErrorPageContent(400));
	} else if (getRequest()->getHeadersFormatValid() == false) {
		if (DEBUG) cout << RED << "Response. Bad request. Invalid headers" << RESET << endl; 
		fillResponse(400, getErrorPageContent(400));
	} else {
		fillResponse(400, getErrorPageContent(400));
	}
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
		if (DEBUG) cout << RED << "Resource not found: " << getRequest()->getUri() << RESET << endl;
		fillResponse(404, getErrorPageContent(404));
		return NULL;
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
	// Check for allowed methods
	const std::vector<string>&	allowed = loc->getAllowedMethods();
	if (allowed.empty()) {
		if (DEBUG) cout << RED << "No methods allowed for this location." << RESET << endl;
		fillResponse(405, getErrorPageContent(405));
		return NULL;
	}
	bool	methodAllowed = false;
	for (size_t i = 0; i < allowed.size(); ++i) {
		if (allowed[i] == getRequest()->getMethod()) {
			methodAllowed = true;
			break;
		}
	}
	if (!methodAllowed) {
		if (DEBUG) cout << RED << "Method " << getRequest()->getMethod() << " not allowed for this location." << RESET << endl;
		fillResponse(405, getErrorPageContent(405));
		return NULL;
	}
	
	return loc;
}

/**
 * @brief Constructs the file system path from location config and request URI.
 * 
 * Determines the root directory (location-specific or server-wide),
 * strips query strings from the URI, and combines them into a full path.
 * Returns a 500 error if no root is configured.
 * 
 * @param loc Pointer to the matched Location
 * @param uri Reference to store the cleaned URI (query string removed)
 * @return The constructed file system path, or empty string on error
 */
/* string		Response::constructPath(const Location* loc) {
	if (DEBUG) cout << ORANGE << "Constructing Path..." << RESET << endl;
	// Determine root
	string			root;
	const string&	locationRoot = loc->getRoot();
	if (!locationRoot.empty())
		root = loc->getRoot();
	else 
		root = _server_config.getRoot();
	// Safety check: root must be configured
	if (root.empty()) {
		if (DEBUG) cout << RED << "Configuration error: No root directive found" << RESET << endl;
		fillResponse(500, getErrorPageContent(500));
		return "";
	}
	// Strip query string from URI
	string		uri = getRequest()->getUri();
	size_t		queryPos = uri.find('?');
	if (queryPos != string::npos) {
		uri = uri.substr(0, queryPos);
	}
	string	path = root + uri;

	if (DEBUG) cout << YELLOW << "Using root: " << root << RESET << endl;
	if (DEBUG) cout << YELLOW << "Using URI: " << uri << RESET << endl;
	if (DEBUG) cout << GREEN << "Resolved path: " << path << RESET << endl;
	
	return path;
} */

/* string Response::constructPath(const Location* loc) {
    if (DEBUG) cout << ORANGE << "Constructing Path..." << RESET << endl;

    // 1) Determine root
    string root;
    if (!loc->getRoot().empty())
        root = loc->getRoot();
    else
        root = _server_config.getRoot();

    if (root.empty()) {
        if (DEBUG) cout << RED << "Configuration error: No root directive found" << RESET << endl;
        fillResponse(500, getErrorPageContent(500));
        return "";
    }

    // 2) Strip query string from URI
    string uri = getRequest()->getUri();
    size_t q = uri.find('?');
    if (q != string::npos)
        uri = uri.substr(0, q);

    // 3) Replace the matched location prefix with its root
    const string locPath = loc->getPath();
    string rel;
    if (locPath == "/") {
        rel = uri; // root location uses full URI
    } else if (uri.compare(0, locPath.length(), locPath) == 0) {
        rel = uri.substr(locPath.length()); // strip location prefix
    } else {
        // Fallback (shouldn't happen if matchPathToLocation() is correct)
        rel = uri;
    }

    // Normalize joining
    if (!rel.empty() && rel[0] == '/')
        rel.erase(0, 1);

    string path = root;
    if (!path.empty() && path[path.size() - 1] != '/')
        path += '/';
    path += rel;

    // 4) If no file part requested (e.g., "/directory"), point to the directory itself
    // generateResponseGet() will then serve index or autoindex
    if (rel.empty() && (path.empty() || path[path.size() - 1] != '/'))
        path += '/';

    if (DEBUG) {
        cout << YELLOW << "Using root: " << root << RESET << endl;
        cout << YELLOW << "Using URI: " << uri << RESET << endl;
        cout << GREEN  << "Resolved path: " << path << RESET << endl;
    }
    return path;
} */

// 3d version
string Response::constructPath(const Location* loc) {
    if (DEBUG) cout << ORANGE << "Constructing Path..." << RESET << endl;

    // Determine roots
    string locRoot = loc->getRoot();
    string srvRoot = _server_config.getRoot();
    string root = !locRoot.empty() ? locRoot : srvRoot;

    if (root.empty()) {
        if (DEBUG) cout << RED << "Configuration error: No root directive found" << RESET << endl;
        fillResponse(500, getErrorPageContent(500));
        return "";
    }

    // Strip query string
    string uri = getRequest()->getUri();
    size_t q = uri.find('?');
    if (q != string::npos) uri = uri.substr(0, q);

    const string	locPath = loc->getPath();
    string			rel;

    // Only strip the location prefix if the location provides its own root.
	// It's instead of alias directive
    // If it uses the server root, keep the full URI to preserve subdirectories.
    if (!locRoot.empty() && locPath != "/" && uri.compare(0, locPath.size(), locPath) == 0) {
		if (DEBUG) cout << "strip the location prefix" << endl;
        rel = uri.substr(locPath.size());
    } else {
        rel = uri;
    }
    //if (!rel.empty() && rel[0] == '/') rel.erase(0, 1);

	//cout << "rel: " << rel << endl;

    // Join root + rel
    string	path = root;
    //if (!path.empty() && path[path.size() - 1] != '/') path += '/';
    path += rel;

    // If requesting the location itself (no file part), point to directory
    if (rel.empty() && (path.empty() || path[path.size() - 1] != '/')) path += '/';

    if (DEBUG) {
        cout << YELLOW << "Using root: " << root << RESET << endl;
        cout << YELLOW << "Using URI: " << uri << RESET << endl;
        cout << GREEN  << "Resolved path: " << path << RESET << endl;
    }
    return path;
}

const Request*	Response::getRequest() { return _request; }

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
	_path.clear();
	_loc = 0;
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

	int	defaultError = 500;
	return getErrorPageContent(defaultError);
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

/**
 * Returns true if the request was handled by CGI (success or error response set)
 * Returns false if not a CGI request or script not found (caller should proceed)
 * 
 * - Extract extension from request path
 * - Look up the interpreter for this extension
 * - Calls CgiHandler constructor
 */
bool		Response::tryServeCgi()
{
	const std::map<string, string>&	cgiMap = _loc->getCgi();
	if (cgiMap.empty()) {
		if (DEBUG) cout << BLUE << "No cgi for location path: " << _loc->getPath() << RESET << endl;
		return false;
	}
	size_t	dotPos = _path.find_last_of('.');
	if (dotPos == string::npos)
		return false;
	string	ext = _path.substr(dotPos + 1);

	std::map<string, string>::const_iterator	it = cgiMap.find(ext);
	if (it == cgiMap.end())
		return false;

	// Allow CGI even if the target file doesn’t exist 
	// if (getPathType(_path) != FILE_PATH) return false;

	if (DEBUG) cout << GREEN << "Executing CGI: " << _path << RESET << endl;
	try {
		CgiHandler	cgi(*this, _path, it->second);
		string		output = cgi.executeCgi();
		if (DEBUG) cout << "cgi output: 200 bytes:\n" << output.substr(0, 199) << endl;
		if (!applyCgiOutput(output)) {
			fillResponse(502, getErrorPageContent(502)); // 502 Bad Gateway
		}
		return true; // Request handled (success or 502)
	} catch (std::exception &e) {
		if (DEBUG) cout << RED << "CGI execution failed: " << e.what() << RESET << endl;
		fillResponse(500, getErrorPageContent(500));
		return true; // Request handled (with 500)
	}
}

/**
 * Prevents sending mismatched Content-Length vs actual body
 * Surfaces truncation (502) instead of silently forwarding a short body.
 */
bool		Response::applyCgiOutput(const std::string &output) {
	if (output.empty()) return false; // invalid CGI response

	// Separate Headers and Body from CGI output
	size_t	headerEnd = output.find("\r\n\r\n");

	size_t	sep_len = 4;
	if (headerEnd == string::npos) {
		headerEnd = output.find("\n\n");
		sep_len = 2;
	}
	if (headerEnd == string::npos) {
		if (DEBUG) cout << RED << "CGI: header/body separator not found" << RESET << endl;
		fillResponse(502, getErrorPageContent(502));
		// To DO to delete fillResponse(200, output);
		return true;
	}

	string	headers = output.substr(0, headerEnd);
	string	body = output.substr(headerEnd + sep_len);
	
	// 2) Parse headers
	if (DEBUG) cout << "CGI. Output raw headers:\n" << headers << endl;
	int					statusCode = 200;
	std::string			statusReason;
	std::string			contentType;
	size_t				contentLengthHeader = static_cast<size_t>(-1);
	std::map<std::string, std::string>	cgiHdrs;
	
	std::istringstream	hs(headers);
	std::string			line;
	while (std::getline(hs, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty()) continue;

		size_t colon = line.find(':');
		if (colon == std::string::npos) continue; // skip malformed
		string	key = line.substr(0, colon);
		string	value = line.substr(colon + 1);

		// Trim leading whitespace
		while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
			value.erase(0, 1);
		
		// Lowercase a copy for comparisons
        std::string	lkey = key;
        for (size_t i = 0; i < lkey.size(); ++i)
			lkey[i] = static_cast<char>(std::tolower(lkey[i]));

		if (key == "status") {
			std::istringstream	statusIss(value);
			int					tempCode;
			if (statusIss >> tempCode) {
				statusCode = tempCode;
				std::string	rest;
                std::getline(statusIss, rest);
                if (!rest.empty()) {
                    if (rest[0] == ' ') rest.erase(0,1);
                    statusReason = rest;
                }
			}
			continue; // do not forward Status header
		}
		if (lkey == "content-type") {
            contentType = value;
        } else if (lkey == "content-length") {
            // store but recompute/validate against body we read
            unsigned long long	n = std::strtoull(value.c_str(), NULL, 10);
            contentLengthHeader = static_cast<size_t>(n);
        } else if (lkey == "connection" || lkey == "transfer-encoding" ||
                   lkey == "keep-alive" || lkey == "proxy-connection" ||
                   lkey == "trailer" || lkey == "upgrade") {
            // TO DO. What is ? drop hop-by-hop headers
            continue;
        } else {
            cgiHdrs[key] = value; // keep original casing for unknown headers
        }
	}

	// 3) Validate/normalize body length w.r.t. CGI Content-Length
    if (contentLengthHeader != static_cast<size_t>(-1)) {
        if (body.size() < contentLengthHeader) {
            if (DEBUG) {
                cout << RED << "CGI body truncated. Have " << body.size()
                     << ", expected " << contentLengthHeader << RESET << endl;
            }
            fillResponse(502, getErrorPageContent(502)); // Bad Gateway
            return true;
        } else if (body.size() > contentLengthHeader) {
            // TO DO what? Truncate surplus bytes if any
            body.resize(contentLengthHeader);
        }
    }

	// 4) Apply headers to response
    if (!contentType.empty())
		_headers["Content-Type"] = contentType;
    // TO DO. What? Recompute Content-Length from the body we will send
    _headers.erase("Content-Length");
    fillResponse(statusCode, body);

	// 5) Optional: keep CGI-provided headers (except the ones we filtered)
    for (std::map<std::string, std::string>::const_iterator it = cgiHdrs.begin(); it != cgiHdrs.end(); ++it) {
        _headers[it->first] = it->second;
    }

	// Prefer CGI reason phrase if provided in "Status:"
    if (!statusReason.empty()) _reasonPhrase = statusReason;

	if (DEBUG) {
        cout << "CGI. Parsed Status: " << statusCode
             << " Body bytes: " << body.size();
        if (contentLengthHeader != static_cast<size_t>(-1))
            cout << " (CGI Content-Length: " << contentLengthHeader << ")";
        cout << endl;
    }
    return true;
}
