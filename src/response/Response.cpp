#include "Response.hpp"

// Parametric constructor
Response::Response(Server &server)
	: _server_config(server),
	  _request(0),
	  _statusCode(200),
	  _reasonPhrase(generateStatusMessage(200)),
	  _contentLength(0)
{
}

Response &Response::operator=(const Response &other)
{
	if (this != &other)
	{
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
void Response::bindRequest(const Request &req)
{
	_request = &req;
}

void printCurrentLocation(const Location *loc)
{
	if (loc)
	{
		std::cout << "Current matched location: " << loc->getPath() << std::endl;
		std::cout << "  Root: " << loc->getRoot() << std::endl;
		std::cout << "  Index: " << loc->getIndex() << std::endl;
		std::cout << "  Autoindex: " << (loc->getAutoindex() ? "on" : "off") << std::endl;
	}
	else
	{
		std::cout << "No location matched." << std::endl;
	}
}

// Helper to find the best matching location
// Returns a pointer to the Location object, or NULL if none found
const Location *Response::matchPathToLocation()
{
	if (!_request)
	{
		return NULL;
	}
	const std::vector<Location> &locations = _server_config.getLocations();
	const Location *bestMatch = NULL;
	size_t bestMatchLen = 0;

	if (DEBUG)
		std::cout << GREEN << "Matching URI: [" << _request->getUri() << "] against " << locations.size() << " locations." << RESET << std::endl;

	for (size_t i = 0; i < locations.size(); ++i)
	{
		const std::string &locPath = locations[i].getPath();
		if (DEBUG)
			std::cout << GREEN << "  Checking location: [" << locPath << "]" << RESET << std::endl;

		// Check if request URI starts with this location path (proper prefix match)
		const std::string &uri = _request->getUri();
		if (DEBUG)
			std::cout << ORANGE << "    Comparing URI: " << uri << " with Location Path: " << locPath << RESET << std::endl;
		const std::string html_ext = ".html";

		if (uri.compare(0, locPath.length(), locPath) == 0)
		{
			// Verify it's a proper path prefix (not just substring match)
			// Special case: "/" matches everything starting with "/"
			// Other locations: character after match must be '/' or end-of-string
			bool isValidPrefix = false;

			if (locPath == "/")
			{
				// Root location matches all URIs starting with "/"
				isValidPrefix = true;
			}
			else if (uri.length() == locPath.length())
			{
				// Exact match (e.g., /about matches /about)
				isValidPrefix = true;
			}
			else if (uri[locPath.length()] == '/')
			{
				// Path continues with '/' (e.g., /about matches /about/page)
				isValidPrefix = true;
			}

			if (isValidPrefix)
			{
				if (DEBUG)
					std::cout << GREEN << "    -> Match found!" << RESET << std::endl;
				// We want the longest match (e.g. location /images/ vs location /)
				if (locPath.length() > bestMatchLen)
				{
					bestMatch = &locations[i];
					bestMatchLen = locPath.length();
					if (DEBUG)
						std::cout << GREEN << "    -> New best match (length " << bestMatchLen << ")" << RESET << std::endl;
				}
			}
			else
			{
				if (DEBUG)
					std::cout << YELLOW << "    -> Substring match but not path prefix (rejected)" << RESET << std::endl;
			}
		}
	}
	printCurrentLocation(bestMatch);
	return bestMatch;
}

// returns the index file name for the matched location or an empty string if none found
std::string Response::getIndexFromLocation()
{
	for (std::vector<Location>::const_iterator it = _server_config.getLocations().begin();
		 it != _server_config.getLocations().end(); ++it)
	{
		if (DEBUG)
		{
			std::cout << "Checking location for index: " << it->getPath() << std::endl;
			std::cout << ORANGE << "Request URI: " << _request->getUri() << RESET << std::endl;
			std::cout << ORANGE << "Location Path: " << it->getPath() << RESET << std::endl;
			std::cout << ORANGE << "Location Index: " << it->getIndex() << RESET << std::endl;
		}
		if (_request->getUri() == it->getPath())
		{
			return "/" + it->getIndex();
		}
	}
	return "";
}

Response::PathType Response::getPathType(std::string const path)
{
	struct stat buffer;
	int result;

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

void Response::generateResponse()
{
	if (!_request)
		return;

	_statusCode = 200;
	_reasonPhrase = generateStatusMessage(_statusCode);
	_responseBody.clear();
	_contentLength = 0;

	const Location *loc = matchPathToLocation();
	if (!loc)
	{
		std::string root = _server_config.getRoot();
		if (DEBUG)
			std::cout << YELLOW << "Using root: " << root << RESET << std::endl;
		std::string uri = _request->getUri() + _server_config.getRoot();
		if (DEBUG)
			std::cout << YELLOW << "Using URI: " << uri << RESET << std::endl;
		std::string path = root + uri;
		std::cout << YELLOW << "Constructed path: " << path << RESET << std::endl;
		if (getPathType(path) == DIRECTORY_PATH)
		{
			if (DEBUG)
				std::cout << BLUE << "Path is a directory." << RESET << std::endl;
			if (path[path.length() - 1] != '/')
				path += "/";

			// Check for index file
			std::string indexFile = "index.html";
			std::string indexPath = path + indexFile;

			if (DEBUG)
				std::cout << BLUE << "Checking index file: " << indexPath << RESET << std::endl;
			if (DEBUG)
				std::cout << YELLOW << "Index to use: " << indexFile << RESET << std::endl;

			if (getPathType(indexPath) == FILE_PATH)
			{
				path = indexPath; // Index exists, serve this file
				if (DEBUG)
					std::cout << GREEN << "Index file exists: " << path << RESET << std::endl;
				std::ostringstream ss;
				std::ifstream file(path.c_str());
				ss << file.rdbuf();
				_responseBody = ss.str();
				_contentLength = _responseBody.size();
				file.close();
				_statusCode = 200;
				_reasonPhrase = generateStatusMessage(_statusCode);
				return;
			}
			else
			{
				if (DEBUG)
					std::cout << BLUE << "Index file not found." << RESET << std::endl;
				// Directory, no index, no autoindex -> Forbidden
				if (DEBUG)
					std::cout << RED << "Directory access forbidden (no index, autoindex off1)" << RESET << std::endl;
				_statusCode = 403;
				_reasonPhrase = generateStatusMessage(_statusCode);
				_responseBody = getErrorPageContent(_statusCode);
				_contentLength = _responseBody.size();
				return;
			}
		}
		else if (getPathType(path) == FILE_PATH)
		{
			if (DEBUG)
				std::cout << GREEN << "File exists at path: " << path << RESET << std::endl;
			std::ostringstream ss;
			std::ifstream file(path.c_str());
			ss << file.rdbuf();
			_responseBody = ss.str();
			_contentLength = _responseBody.size();
			file.close();
			_statusCode = 200;
			_reasonPhrase = generateStatusMessage(_statusCode);
			return;
		}
		else
		{
			if (DEBUG)
				std::cout << RED << "No location matched." << RESET << std::endl;
			_statusCode = 404;
			_reasonPhrase = generateStatusMessage(_statusCode);
			_responseBody = getErrorPageContent(_statusCode);
			_contentLength = _responseBody.size();
		}
		return;
	}

	// Check for allowed methods
	const std::vector<std::string> &allowed = loc->getAllowedMethods();
	if (!allowed.empty())
	{
		bool methodAllowed = false;
		for (size_t i = 0; i < allowed.size(); ++i)
		{
			if (allowed[i] == _request->getMethod())
			{
				methodAllowed = true;
				break;
			}
		}
		if (!methodAllowed)
		{
			if (DEBUG)
				std::cout << RED << "Method " << _request->getMethod() << " not allowed for this location." << RESET << std::endl;
			_statusCode = 405;
			_reasonPhrase = generateStatusMessage(_statusCode);
			_responseBody = getErrorPageContent(_statusCode);
			_contentLength = _responseBody.size();
			return;
		}
	}

	// Check for redirection
	if (loc->getReturnCode() != 0)
	{
		_statusCode = loc->getReturnCode();
		_reasonPhrase = generateStatusMessage(_statusCode);
		_headers["Location"] = loc->getReturnUrl();
		_responseBody = "<html><body><h1>" + toString(_statusCode) + " " + _reasonPhrase + "</h1></body></html>";
		_contentLength = _responseBody.size();
		return;
	}

	// Construct Path: root + remaining URI after location prefix
	std::string root = !loc->getRoot().empty() ? loc->getRoot() : _server_config.getRoot();
	if (root[0] != '/' && !loc->getRoot().empty() && root.find(_server_config.getRoot()) == std::string::npos)
		root = _server_config.getRoot() + "/" + root;
	if (DEBUG)
		std::cout << YELLOW << "Using root: " << root << RESET << std::endl;
	std::string uri = _request->getUri();
	if (DEBUG)
		std::cout << YELLOW << "Using URI: " << uri << RESET << std::endl;

	// Strip location prefix from URI to get the remaining path
	// e.g., location /gallery/ -> /gallery
	std::string locPath = loc->getPath();
	std::string remainingUri;
	if (locPath == "/")
	{
		// Root location: use full URI
		remainingUri = uri;
	}
	else if (uri.compare(0, locPath.length(), locPath) == 0)
	{
		// URI starts with location path: strip it
		remainingUri = uri.substr(locPath.length());
		if (remainingUri.empty())
		{
			remainingUri = "/";
		}
	}
	else
	{
		// Shouldn't happen if matching worked correctly
		remainingUri = uri;
	}

	if (DEBUG)
		std::cout << YELLOW << "Remaining URI after location: " << remainingUri << RESET << std::endl;
	std::string path = root + remainingUri;
	if (DEBUG)
		std::cout << GREEN << "Resolved path: " << path << RESET << std::endl;

	// Check if path exists
	PathType pathType = getPathType(path);

	// CRITICAL: If location exists in config but path doesn't exist on disk → 404
	if (pathType == NOT_EXIST)
	{
		// Special case: if URI exactly matches location path and location has index
		// Try serving the index file directly (e.g., /about → about.html)
		if (uri == loc->getPath() && !loc->getIndex().empty())
		{
			std::string indexFile = loc->getIndex();
			std::string indexPath = root + "/" + indexFile;
			if (DEBUG)
				std::cout << BLUE << "Path doesn't exist, trying location index: " << indexPath << RESET << std::endl;

			if (getPathType(indexPath) == FILE_PATH)
			{
				path = indexPath;
				if (DEBUG)
					std::cout << GREEN << "Using location index file: " << path << RESET << std::endl;
				pathType = FILE_PATH;
			}
			else
			{
				// Index file also doesn't exist → 404
				if (DEBUG)
					std::cout << RED << "Location index file not found: " << indexPath << RESET << std::endl;
				_statusCode = 404;
				_reasonPhrase = generateStatusMessage(_statusCode);
				_responseBody = getErrorPageContent(_statusCode);
				_contentLength = _responseBody.size();
				return;
			}
		}
		else
		{
			// Path doesn't exist and no index fallback → 404
			if (DEBUG)
				std::cout << RED << "Path not found: " << path << RESET << std::endl;
			_statusCode = 404;
			_reasonPhrase = generateStatusMessage(_statusCode);
			_responseBody = getErrorPageContent(_statusCode);
			_contentLength = _responseBody.size();
			return;
		}
	}

	// Handle Directory
	if (pathType == DIRECTORY_PATH)
	{
		if (DEBUG)
			std::cout << BLUE << "Path is a directory.1" << RESET << std::endl;
		if (path[path.length() - 1] != '/')
			path += "/";
		// if autoindex true
		//
		bool isIndexPresent = loc->getIndex().empty() ? false : true;
		bool isAutoindexPresent = loc->getAutoindex() ? true : false;
		// Check for index file
		std::string indexFile = isIndexPresent ? loc->getIndex() : "";

		// std::string targetPath = path + indexFile;
		std::string targetPath = indexFile.empty() ? isAutoindexPresent ? path : "" : path + indexFile;

		if (DEBUG)
			std::cout << BLUE << "Checking targetPath file1: " << targetPath << RESET << std::endl;
		if (DEBUG)
			std::cout << YELLOW << "Index to use: " << indexFile << RESET << std::endl;

		if (getPathType(targetPath) == FILE_PATH)
		{
			path = targetPath; // Index exists, serve this file
			if (DEBUG)
				std::cout << GREEN << "Index file exists1: " << path << RESET << std::endl;
		}
		else
		{
			if (DEBUG)
				std::cout << BLUE << "Index file not found." << RESET << std::endl;
			// No index file found. Check Autoindex.
			if (loc->getAutoindex())
			{
				if (DEBUG)
					std::cout << BLUE << "Autoindex is ON. Generating listing for: " << path << RESET << std::endl;
				std::string body;
				size_t bodyLen = 0;
				if (buildHtmlIndexTable(path, body, bodyLen) == 0)
				{
					_statusCode = 200;
					_reasonPhrase = generateStatusMessage(_statusCode);
					_responseBody = body;
					_contentLength = bodyLen;
					return;
				}
				if (DEBUG)
					std::cout << RED << "Autoindex generation failed." << RESET << std::endl;
				_statusCode = 500;
				_reasonPhrase = generateStatusMessage(_statusCode);
				_responseBody = getErrorPageContent(_statusCode);
				_contentLength = _responseBody.size();
				return;
			}
			else
			{
				// Directory, no index, no autoindex -> Forbidden
				if (DEBUG)
					std::cout << RED << "Directory access forbidden (no index, autoindex off2)" << RESET << std::endl;
				_statusCode = 403;
				_reasonPhrase = generateStatusMessage(_statusCode);
				_responseBody = getErrorPageContent(_statusCode);
				_contentLength = _responseBody.size();
				return;
			}
		}
	}

	// 3. Serve File
	if (DEBUG)
		std::cout << BLUE << "Serving file: " << path << RESET << std::endl;
	std::ifstream file(path.c_str());

	if (loc->getAutoindex())
	{
		if (DEBUG)
			std::cout << BLUE << "Autoindex is ON. Generating listing for: " << path << RESET << std::endl;
		std::string body;
		size_t bodyLen = 0;
		if (buildHtmlIndexTable(path, body, bodyLen) == 0)
		{
			_statusCode = 200;
			_reasonPhrase = generateStatusMessage(_statusCode);
			_responseBody = body;
			_contentLength = bodyLen;
			return;
		}
		if (DEBUG)
			std::cout << RED << "Autoindex generation failed." << RESET << std::endl;
		_statusCode = 500;
		_reasonPhrase = generateStatusMessage(_statusCode);
		_responseBody = getErrorPageContent(_statusCode);
		_contentLength = _responseBody.size();
		return;
	}
	else if (!file.is_open())
	{
		if (DEBUG)
			std::cout << RED << "File not found: " << path << RESET << std::endl;
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
}

short Response::getStatusCode() const { return _statusCode; }

size_t Response::getContentLength() const { return _contentLength; }

std::string Response::getResponseBody() const
{
	return _responseBody;
}

/** nulls the Request* before new HTTP request-response cycle */
void Response::reset()
{
	_request = 0;
}

Server &Response::getServerConfig()
{
	return _server_config;
}

std::string Response::getReasonPhrase() const
{
	return _reasonPhrase;
}

const std::map<std::string, std::string> &Response::getHeaders() const
{
	return _headers;
}

std::string Response::getErrorPageContent(int code)
{
	const std::map<int, std::string> &errorPages = _server_config.getErrorPages();
	std::map<int, std::string>::const_iterator it = errorPages.find(code);

	if (it != errorPages.end())
	{
		std::ifstream file(it->second.c_str());
		if (file.is_open())
		{
			std::ostringstream ss;
			ss << file.rdbuf();
			return ss.str();
		}
	}
	return "";
}