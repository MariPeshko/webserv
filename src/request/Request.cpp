#include "Request.hpp"

Request::Request() :
	_validFormatReqLine(false),
	_validFormatHeaders(false),
	_method(INVALID)
{ }

Request::~Request() { }

void Request::setMethod(const std::string &method) {
	if (method.size() == 0) {
		_method = INVALID;
		return;
	}
	if (method == "GET") {
		_method = GET;
	} else if (method == "POST") {
		_method = POST;
	} else if (method == "DELETE") {
		_method = DELETE;
	} else {
		_method = INVALID;
	}
}

void Request::setUri(const std::string &uri) {
	_uri = uri;
}

void Request::setVersion(const std::string &version) {
	_httpVersion = version;
}

void Request::addHeader(const std::string &key, const std::string &value) {
	_headers[key] = value;
}

void Request::setBody(const std::string &body) {
	_body = body;
}

void	Request::setRequestLineFormatValid(bool value) {
	_validFormatReqLine = value;
}

void	Request::setHeadersFormatValid(bool value) {
	_validFormatHeaders = value;
}

/**
 * When a request line format is invalid it returns false.
 * For example, a request line is empty, method is INVALID,
 * the number of words aren't 3.
 */
bool		Request::getRequestLineFormatValid() const {
	return _validFormatReqLine;
}

bool		Request::getHeadersFormatValid() const {
	return _validFormatHeaders;
}

std::string	Request::getMethod() const {
	switch (_method) {
		case GET: return "GET";
		case POST: return "POST";
		case DELETE: return "DELETE";
		default: return "";
	}
}

std::string Request::getUri() const {
	return _uri;
}

std::string Request::getVersion() const {
	return _httpVersion;
}

std::map<std::string, std::string>	Request::getHeaders() const {
	return _headers;
}

// checks if the key "content-length" is present
bool	Request::isContentLengthHeader() const {
	if (_headers.find("content-length") == _headers.end())
		return false;
	else
		return true;
}

const std::string&	Request::getHeaderValue(const std::string header_name) const {
	
	static const std::string	empty = "";
	std::map<std::string, std::string>::const_iterator it;

	it = _headers.find(header_name);
	if (it == _headers.end()) {
		return empty;
	} else {
		return it->second;
	}
}

std::string Request::getBody() const {
	return _body;
}
