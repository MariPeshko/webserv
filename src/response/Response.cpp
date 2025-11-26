#include "Response.hpp"

#define GREEN "\033[32m"
#define RESET "\033[0m"

// Parametric constructor
Response::Response(Server& server)
    : _server_config(server),
      _request(0),
      _statusCode(200),
      _reasonPhrase("OK"),
      _contentLength(0)
{ }

Response::~Response() { }

// call after parsing
void Response::bindRequest(const Request& req) {
    _request = &req;
}

static bool isDirectory(const std::string& path) {
    struct stat s;
    if (stat(path.c_str(), &s) == 0) {
        if (s.st_mode & S_IFDIR) {
            return true;
        }
    }
    return false;
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

    std::cout << GREEN << "Matching URI: [" << _request->getUri() << "] against " << locations.size() << " locations." << RESET << std::endl;

    for (size_t i = 0; i < locations.size(); ++i) {
        const std::string& locPath = locations[i].getPath();
        std::cout << GREEN << "  Checking location: [" << locPath << "]" << RESET << std::endl;
        
        // Check if request URI starts with this location path
        if (_request->getUri().find(locPath) == 0) {
            std::cout << GREEN << "    -> Match found!" << RESET << std::endl;
            // We want the longest match (e.g. location /images/ vs location /)
            if (locPath.length() > bestMatchLen) {
                bestMatch = &locations[i];
                bestMatchLen = locPath.length();
                std::cout << GREEN << "    -> New best match (length " << bestMatchLen << ")" << RESET << std::endl;
            }
        }
    }
    return bestMatch;
}

void Response::generatePath() {
	for (std::vector<Location>::const_iterator it = _server_config.getLocations().begin(); 
			it != _server_config.getLocations().end(); ++it) {
		// if (request.getUri().find(it->getPath()) == 0) {
			std::cout << "location: " << it->getPath() << std::endl;
			// break;
		// }
	}
	
	// std::cout << " test: " << _server_config.getLocations().begin()->_path << std::endl;
}

void Response::generateResponse() {
    if (!_request) return;

    _statusCode = 200;
    _reasonPhrase = "OK";

    const Location* loc = matchPathToLocation();
    std::string root;
    std::string uri = _request->getUri();
    std::string path;

    if (loc) {
        std::cout << GREEN << "Matched location: " << loc->getPath() << RESET << std::endl;
        // Determine root, use server root if location root not set
		if (!loc->getRoot().empty()) {
            root = loc->getRoot();
        } else {
            root = _server_config.getRoot();
        }
        std::cout << GREEN << "Root: " << root << RESET << std::endl;

        std::string locPath = loc->getPath();
		std::cout << GREEN << "Location path: " << locPath << RESET << std::endl;
		std::cout << GREEN << "Request URI: " << uri << RESET << std::endl;
        // Alias behavior: replace location part with root
        if (uri.find(locPath) == 0) {
            std::string remainder = uri.substr(locPath.length());
            path = root + remainder;
            std::cout << GREEN << "Alias path construction: " << root << " + " << remainder << " = " << path << RESET << std::endl;
        } else {
            path = root + uri;
            std::cout << GREEN << "Standard path construction: " << root << " + " << uri << " = " << path << RESET << std::endl;
        }

        // Handle Directory Index
        if (isDirectory(path)) {
            std::cout << GREEN << "Path is a directory, checking index..." << RESET << std::endl;
            if (!path.empty() && path[path.length() - 1] != '/') {
                path += "/";
            }
            if (!loc->getIndex().empty()) {
                path += loc->getIndex();
                std::cout << GREEN << "Using location index: " << loc->getIndex() << RESET << std::endl;
            } else {
                path += "index.html";
                std::cout << GREEN << "Using default index: index.html" << RESET << std::endl;
            }
        }
    } else {
        std::cout << GREEN << "No location matched, using server root." << RESET << std::endl;
        root = _server_config.getRoot();
		// path = uri == "/" ? root : root + uri;
		path = root + uri;
        std::cout << GREEN << "Path construction: " << root << " + " << uri << " = " << path << RESET << std::endl;
        if (isDirectory(path)) {
            std::cout << GREEN << "Path is a directory, checking index..." << RESET << std::endl;
            if (!path.empty() && path[path.length() - 1] != '/') {
                path += "/";
            }
            path += "index.html";
            std::cout << GREEN << "Using default index: index.html" << RESET << std::endl;
        }
    }

    std::cout << "Serving file: " << path << std::endl;

    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        _statusCode = 404;
        _reasonPhrase = "Not Found";
        _responseBody = "<html><body><h1>404 Not Found</h1></body></html>";
        _contentLength = _responseBody.size();
        std::cout << "Failed to open file: " << path << std::endl;
        return;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    _responseBody = ss.str();
    file.close();
    _contentLength = _responseBody.size();
}

short Response::getStatusCode() const { return _statusCode; }

size_t Response::getContentLength() const { return _contentLength; }

std::string Response::getResponseBody() const {
    return _responseBody;
}

Server& Response::getServerConfig() {
	return _server_config;
}

std::string Response::getReasonPhrase() const {
	return _reasonPhrase;
}