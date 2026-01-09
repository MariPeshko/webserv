#include "HttpContext.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::string;

// Parametic constructor
HttpContext::HttpContext(Connection &conn, Server &server) :
	_conn(conn),
	_server_config(server),
	_request(),
	_response(server),
	_state(REQUEST_LINE),
	_expectedBodyLen(0),
	_chunkState(READING_CHUNK_SIZE),
	_chunkSize(0),
	_accumulatedBodySize(0),
	_responseBuffer(""),
	_bytesSent(0),
	_draining(false),
	_drainStart(0)
{ }

// Copy constructor
HttpContext::HttpContext(const HttpContext &other) :
	_conn(other._conn),
	_server_config(other._server_config),
	_request(other._request),
	_response(other._server_config),
	_state(other._state),
	_expectedBodyLen(other._expectedBodyLen),
	_chunkState(other._chunkState),
	_chunkSize(other._chunkSize),
	_accumulatedBodySize(other._accumulatedBodySize),
	_responseBuffer(other._responseBuffer),
	_bytesSent(other._bytesSent),
	_draining(other._draining),
	_drainStart(other._drainStart)
{ }

HttpContext::~HttpContext() {}

Connection	&HttpContext::connection() { return _conn; }
Server		&HttpContext::server() { return _server_config; }
Request		&HttpContext::request() { return _request; }
Response	&HttpContext::response() { return _response; }

/**
 * REQUEST PARSER STATE MACHINE. It processes the _request_buffer
 * and transitions the client's state. It will loop as long as it can
 * make progress (e.g., parsing both request line and headers if they
 * are both in the buffer).
 */
void	HttpContext::requestParsingStateMachine()
{
	string&	buf = connection().getBuffer();
	bool	can_parse = true;

	while (can_parse) {
		switch (_state) {
			case REQUEST_LINE: {
				if (!checkRequestLineSize(buf)) {
					_state = REQUEST_ERROR;
					request().setStatusCode(414); // URI Too Long
					request().ifConnNotPresent();
					request().setVersion("HTTP/1.0");
					can_parse = false;
					break;
				}

				if (!findAndParseReqLine(buf)) {
					if (_state == REQUEST_ERROR) {
						if (CTX_DEBUG) cout << RED << "ParseReqLine error" << RESET << endl;
						if (REQ_DEBUG) PrintUtils::printRequestLineInfo(request());
						if (REQ_DEBUG) cout << RESET << endl;
						request().ifConnNotPresent();
						can_parse = false;
						break;
					}
					// Otherwise, we just need more data, so we wait
					can_parse = false;
					break;
				} else {
					// If successful, the state is READING_HEADERS, continue parsing
					if (REQ_DEBUG) PrintUtils::printRequestLineInfo(request());
					_state = READING_HEADERS;
					continue; // Continue to READING_HEADERS case
				}
			}
			case READING_HEADERS : {
				if (!checkHeaderBlockSize(buf)) {
					if (REQ_DEBUG) cout << RED << "limit of headers" << RESET << endl;
					_state = REQUEST_ERROR;
					request().setStatusCode(431); // Request Header Fields Too Large
					request().ifConnNotPresent();
					request().setVersion("HTTP/1.0");
					can_parse = false;
					break;
				}
				if (!findAndParseHeaders(buf)) {
					request().ifConnNotPresent();
					can_parse = false; break;
				}
				if (REQ_DEBUG) PrintUtils::printRequestHeaders(request());
				if (isBodyToRead()) {
					if (CTX_DEBUG) cout << YELLOW << "requestParsingStateMachine: we have a body to read" << RESET << endl;
					continue;
				} else {
					if (CTX_DEBUG) cout << YELLOW << "requestParsingStateMachine: no body to read" << RESET << endl;
					continue;
				}
			}
			case READING_FIXED_BODY : {
				if (!findAndParseFixBody(buf)) {
					can_parse = false; break;
				} else {
					continue;
				}
			}
			case READING_CHUNKED_BODY : {
				while (chunkedBodyStateMachine(buf)) { // The body of the loop can be empty
					}
				if (_state != REQUEST_COMPLETE && _state != REQUEST_ERROR) {
					can_parse = false; // Pause parsing if we're waiting for more data
				}
				continue;
			}
			case REQUEST_COMPLETE : {
				if (REQ_DEBUG_BODY) { if (request().getBody().size() != 0)
						PrintUtils::printBody(request()); }
				if (CTX_DEBUG) cout << ORANGE << "REQUEST PARSING IS COMPLETE" << RESET << endl;
				can_parse = false;
				break;
			}
			case REQUEST_ERROR  : {
				if (CTX_DEBUG) cout << RED << "REQUEST ERROR" << RESET << endl;
				can_parse = false;
				break;
			}
			default : {
				cerr << "HttpContext class message: Unknown state of requst parsing" << endl;
				_state = REQUEST_ERROR;
				can_parse = false;
				break;
			}
		}
	}
}

// Validate host from absolute URI if present
// absolute URI is http://localhost:8080/ or http://127.0.0.1:8080/
bool	HttpContext::validateHost()
{
	if (CTX_DEBUG) cout << YELLOW << "ReqLineTrue: Validate host from absolute URI if present" << RESET << endl;
	
	const string&	req_host_full = request().getHost();
	if (!req_host_full.empty()) {
		string	req_hostname = req_host_full;
		int req_port = -1;
		// Separate hostname and port from the request's host string
		size_t	colon_pos = req_host_full.find(':');
		if (colon_pos != std::string::npos) {
			req_hostname = req_host_full.substr(0, colon_pos);
			std::stringstream	ss(req_host_full.substr(colon_pos + 1));
			if (!(ss >> req_port)) {
				if (CTX_DEBUG) cerr << "Invalid port in request host: " << req_host_full << endl;
				return false;
			}
		}
		// Check if the port matches the server's listening port
		if (req_port != -1 && req_port != _server_config.getPort()) {
			if (CTX_DEBUG) cerr << YELLOW << "Port mismatch. Request: " << req_port << ", Server: " << _server_config.getPort() << endl;
			request().setUri("/" + req_hostname + request().getUri());
			if (CTX_DEBUG) cerr << YELLOW << "New request URI is: " << request().getUri() << RESET << endl;
			return false;
		}
		// Check if the hostname matches one of the server's names
		const std::vector<std::string>&	server_names = _server_config.getServerNames();
		bool							host_match = false;
		for (size_t i = 0; i < server_names.size(); ++i) {
			if (server_names[i] == req_hostname) {
				host_match = true;
				break;
			}
		}
		if (!host_match) {
			if (CTX_DEBUG) cerr << "Hostname mismatch, modifying URI: " << req_hostname << endl;
			request().setUri("/" + req_hostname + request().getUri());
			if (CTX_DEBUG) cerr << "New request URI is: " << request().getUri() << endl;
			return false;
		}
	}
	if (CTX_DEBUG) cout << YELLOW << "Host validated succesfully" << RESET << endl;
	return true;
}

bool	HttpContext::findAndParseReqLine(std::string &buf)
{
	size_t	pos = buf.find("\r\n");
	if (pos == string::npos)
		return false;

	string	line = buf.substr(0, pos);
	buf.erase(0, pos + 2);

	if (HttpParser::parseRequestLine(line, request()) == true) {
		// Validate host from absolute URI if present
		if (request().getHost().size() > 0) {
			if (validateHost() == false) {
				_state = REQUEST_ERROR;
				return false;
			}
		}
		_state = READING_HEADERS;
		return true;
	} else {
		_state = REQUEST_ERROR;
		return false;
	}
}

bool	HttpContext::findAndParseHeaders(string &buf)
{
	size_t	pos = buf.find("\r\n\r\n");
	if (pos == string::npos) {
		return false;
	}

	string	rawHeaders = buf.substr(0, pos);
	buf.erase(0, pos + 4);
	if (HttpParser::parseHeaders(rawHeaders, request()) == false) {
		_state = REQUEST_ERROR;
		return false;
	} else {
		return true;
	}
}

/**
 * RFC note: If a message is received with both a Transfer-Encoding
 * header field and a Content-Length header field, the latter MUST be ignored.
 *
 * Certain request methods like GET and DELETE typically do not
 * have a message body.
 */
bool	HttpContext::isBodyToRead()
{
	string	method = request().getMethod();
	if (method == "GET" || method == "DELETE") {
		request().setChunked(false);
		_state = REQUEST_COMPLETE;
		return false;
	}

	if (request().isTransferEncodingHeader()) {
		if (request().getHeaderValue("transfer-encoding") != "chunked") {
			_state = REQUEST_ERROR;
			return false;
		}
		_state = READING_CHUNKED_BODY;
		return true;
	} else if (request().isContentLengthHeader()) {
		const string&	cl = request().getHeaderValue("content-length");
		if (cl.empty()) {
			_state = REQUEST_COMPLETE;
			return false;
		}

		size_t	contentLength = 0;
		if (!HttpParser::safeParseContentLength(cl, contentLength)) {
			_state = REQUEST_ERROR;
			request().setStatusCode(400); // invalid Content-Length
			return false;
		}

		// Now check against client_max_body_size
		if (!checkBodySizeLimit(contentLength)) {
			_state = REQUEST_ERROR;
			request().setStatusCode(413);
			return false;
		}
		if (contentLength == 0) {
			_state = REQUEST_COMPLETE;
			return false;
		} else {
			_state = READING_FIXED_BODY;
			_expectedBodyLen = contentLength;
			return true;
		}
	}
	// No body-defining headers found
	_state = REQUEST_COMPLETE;
	return false;
}

bool	HttpContext::findAndParseFixBody(std::string &buf)
{
	size_t			remaining = _expectedBodyLen - request().getBody().size();
	const size_t	take = std::min(remaining, buf.size());
	if (take == 0) { // need more data from socket
		return false;
	}
	HttpParser::appendToBody(buf, take, request());
	buf.erase(0, take);
	if (request().getBody().size() == _expectedBodyLen)	{
		_state = REQUEST_COMPLETE;
		return true;
	} else {
		return false;
	}
}

/**
 * track the accumulated body size during chunk processing
 */
bool	HttpContext::chunkedBodyStateMachine(std::string &buf)
{
	if (_chunkState == READING_CHUNK_SIZE) {
		size_t	pos = buf.find("\r\n");
		if (pos == string::npos) { // wait more data
			return false;
		}
		string	size_hex = buf.substr(0, pos);
		// Parse Chunk Size: Convert the hexadecimal string to an integer (chunk_size).
		if (!HttpParser::cpp98_hexaStrToInt(size_hex, _chunkSize)) {
			cerr << "Chunked body. Invalid Hexadecimal size" << endl;
			_state = REQUEST_ERROR;
			return false;
		}
		buf.erase(0, pos + 2); // Erase hex size and \r\n
		if (_chunkSize == 0) { // End of body. Look for final \r\n
			_chunkState = READING_CHUNK_TRAILER;
		} else {
			_chunkState = READING_CHUNK_DATA;
		}
		return true;
	}

	if (_chunkState == READING_CHUNK_DATA) {
		if (buf.size() < _chunkSize) { // Need more data for chunk body
			return false;
		}
		// Check if adding this chunk would exceed the limit
		if (!checkBodySizeLimit(_accumulatedBodySize + _chunkSize)) {
			_state = REQUEST_ERROR;
			request().setStatusCode(413);
			return false;
		}

		HttpParser::appendToBody(buf, _chunkSize, request());
		_accumulatedBodySize += _chunkSize;
		buf.erase(0, _chunkSize);
		_chunkState = READING_CHUNK_TRAILER;
		return true;
	}

	if (_chunkState == READING_CHUNK_TRAILER) {
		if (buf.size() < 2) { // Need more data for \r\n
			return false;
		}
		if (buf.rfind("\r\n", 0) != 0) { // Check for \r\n at the start
			_state = REQUEST_ERROR;
			return false;
		}
		buf.erase(0, 2);
		if (_chunkSize == 0) {
			_state = REQUEST_COMPLETE;
			return false;
		} else {
			_chunkState = READING_CHUNK_SIZE;
			return true;
		}
	}
	return false;
}

/**
 * Checks if the received request is complete.
 * This is a basic implementation. A robust one would also check Content-Length.
 * @return true if the HTTP headers are complete, false otherwise.
 */
bool HttpContext::isRequestComplete() const {
	if (_state == REQUEST_COMPLETE)
		return true;
	else
		return false;
}

bool HttpContext::isRequestError() const {
	if (_state == REQUEST_ERROR)
		return true;
	else
		return false;
}

void	HttpContext::resetState() {
	_request = Request();
	response().reset();
	connection().getBuffer().clear();
	_state = REQUEST_LINE;
	_expectedBodyLen = 0;
	_chunkState = READING_CHUNK_SIZE;
	_chunkSize = 0;
	_responseBuffer = "";
	_bytesSent = 0;
}

void	HttpContext::buildResponseString()
{
	std::string			body = _response.getResponseBody();
	short				status_code = _response.getStatusCode();
	size_t				content_length = _response.getContentLength();
	const std::string&	reason_phrase = _response.getReasonPhrase();
	std::ostringstream	oss;

	// 1. Status Line
	string	version = _request.getVersion();
	if (version.empty())
		version = "HTTP/1.0";
	oss << version << " " << status_code << " " << reason_phrase << "\r\n";
	
	// FIX: For error responses, always use Connection: close
	string connectionValue = _request.getHeaderValue("connection");
	if (status_code >= 400 || connectionValue.empty()) {
		connectionValue = "close";
	}
	oss << "Connection: " << connectionValue << "\r\n";

	// 2. Headers
	oss << "Content-Length: " << content_length << "\r\n";
	const std::map<string, string>&	headers = response().getHeaders();
	for (std::map<string, string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
	{
		oss << it->first << ": " << it->second << "\r\n";
	}
	// 3. Empty Line (End of headers)
	oss << "\r\n";
	oss << body;

	setResponseBuffer(oss.str());
}

HttpContext::e_parse_state	HttpContext::getParserState() const {
	return _state;
}

// disabled operator, it is in private
HttpContext&	HttpContext::operator=(const HttpContext &other) {
	(void)other;
	return *this;
}

std::string		HttpContext::getResponseBuffer() const {
	return _responseBuffer;
}

size_t			HttpContext::getBytesSent() const {
	return _bytesSent;
}

void			HttpContext::setResponseBuffer(const std::string &buffer) {
	_responseBuffer = buffer;
	if (RESP_DEBUG) cout << "setResponseBuffer(): ";
	if (RESP_DEBUG) cout << "METHOD / URI: " << _request.getMethod() << " " << _request.getUri() << endl;
	if (RESP_DEBUG) cout << "Response. 100 lines:\n";
	if (RESP_DEBUG) cout << YELLOW << _responseBuffer.substr(0, 100) << RESET << endl;
	if (RESP_DEBUG) cout << "|||" << endl;
	_bytesSent = 0;
}

void			HttpContext::addBytesSent(size_t bytes) {
	_bytesSent += bytes;
}

bool			HttpContext::isResponseComplete() const {
	return _bytesSent >= _responseBuffer.size();
}

/**
 * Check if the buffer exceeds the request line size limit.
 * If \r\n is found, check only the request line portion.
 * If \r\n is not found yet, check the entire buffer (waiting for complete line).
 */
bool	HttpContext::checkRequestLineSize(const std::string &buf)
{
	size_t	pos = buf.find("\r\n");
	size_t	sizeToCheck;
	
	if (pos != string::npos) {
		// We have a complete request line, check only its size
		sizeToCheck = pos;
	} else {
		// Still waiting for \r\n, check the entire buffer
		sizeToCheck = buf.size();
	}
	
	if (sizeToCheck > MAX_REQUEST_LINE_SIZE) {
		if (CTX_DEBUG) cerr << RED << "Request line exceeds maximum size: " 
			<< sizeToCheck << " > " << MAX_REQUEST_LINE_SIZE << RESET << endl;
		return false;
	}
	return true;
}

/**
 * Check if the header portion exceeds the header block size limit.
 * Only checks the actual headers, not body data mixed in the buffer.
 */
bool	HttpContext::checkHeaderBlockSize(const std::string &buf)
{
	size_t headerEnd = buf.find("\r\n\r\n");
	size_t sizeToCheck;
	
	if (headerEnd != string::npos) {
		sizeToCheck = headerEnd;
	} else {
		sizeToCheck = buf.size();
		// TO DO: why multiplying by two?
		if (sizeToCheck > MAX_HEADER_BLOCK_SIZE * 2) {
			if (CTX_DEBUG) cerr << RED << "Potential attack: huge buffer without header termination" << RESET << endl;
			return false;
		}
	}
	if (sizeToCheck > MAX_HEADER_BLOCK_SIZE) {
		if (CTX_DEBUG) cerr << RED << "Header block exceeds maximum size: " 
			<< sizeToCheck << " > " << MAX_HEADER_BLOCK_SIZE << RESET << endl;
		return false;
	}
	return true;
}

/**
 * - Check location-specific limit first
 * - Fall back to server-level limit
 * - If maxBodySizeStr is empty then no limit set
 * - Parse the size string (e.g., "1m", "500k", "1024")
 * 
 * Return true if size is valid.
 */
bool	HttpContext::checkBodySizeLimit(size_t contentLength)
{
	const Location*	matchedLocation = findMatchingLocation();
	string			maxBodySizeStr;
	
	if (matchedLocation && !matchedLocation->getClientMaxBodySize().empty()) {
		maxBodySizeStr = matchedLocation->getClientMaxBodySize();
	} else {
		maxBodySizeStr = _server_config.getClientMaxBodySize();
	}
	if (maxBodySizeStr.empty()) {
		return true; 
	}
	size_t maxBodySize = HttpParser::parseSizeString(maxBodySizeStr);
	
	return contentLength <= maxBodySize;
}

// TO DO: mpeshko: to compare it to matchPathToLocation(). Can we use
// one method in two places?
const Location* HttpContext::findMatchingLocation()
{
	const std::vector<Location>&	locations = _server_config.getLocations();
	const Location*					bestMatch = NULL;
	size_t							bestMatchLen = 0;
	
	const string&					uri = request().getUri();
	
	for (size_t i = 0; i < locations.size(); ++i) {
		const string&	locPath = locations[i].getPath();
		
		if (uri.find(locPath) == 0) { // URI starts with location path
			if (locPath.length() > bestMatchLen) {
				bestMatch = &locations[i];
				bestMatchLen = locPath.length();
			}
		}
	}
	
	return bestMatch;
}

void	HttpContext::startDraining() {
	_draining = true;
	_drainStart = time(NULL);
}

void	HttpContext::stopDraining() {
	_draining = false;
	_drainStart = 0;
}

bool	HttpContext::isDraining() const { return _draining; }

bool	HttpContext::hasDrainTimedOut(time_t now, time_t timeoutSec) const {
	if (!_draining)
		return false;
	return (now - _drainStart) >= timeoutSec;
}
