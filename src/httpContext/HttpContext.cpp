#include "HttpContext.hpp"
#include "PrintUtils.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

// Parametic constructor
HttpContext::HttpContext(Connection& conn, Server& server) :
	_conn(conn),
	_server_config(server),
	_request(),
	_response(server),
	_state(REQUEST_LINE),
	_expectedBodyLen(0),
	_chunkState(READING_CHUNK_SIZE),
	_chunkSize(0)
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
	_chunkSize(other._chunkSize)
{ }

HttpContext::~HttpContext() { }

Connection& HttpContext::connection() { return _conn; }
Server&     HttpContext::server()     { return _server_config; }
Request&    HttpContext::request()    { return _request; }
Response&   HttpContext::response()   { return _response; }

bool	HttpContext::findAndParseHeaders(string &buf) {
	size_t pos = buf.find("\r\n\r\n");
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
bool	HttpContext::isBodyToRead() {

	std::string	method = request().getMethod();
	if (method == "GET" || method == "DELETE") {
		request().setChunked(false);
		_state = REQUEST_COMPLETE; return false;
	}

	if(request().isTransferEncodingHeader()) {
		if (request().getHeaderValue("transfer-encoding") != "chunked") {
			_state = REQUEST_ERROR; return false;
		}
		//cout << YELLOW << "isBodyToRead(): Transfer-Encoding header is here" << endl;
		_state = READING_CHUNKED_BODY; return true;
	} else if(request().isContentLengthHeader()) {

		const string& cl = request().getHeaderValue("content-length");
		if (cl.empty()) {
		_state = REQUEST_COMPLETE; return false;
		}
		// Reject non-digit characters early and manual accumulation
		size_t	need = 0;
		bool	ok = true;
		for (size_t i = 0; i < cl.size(); ++i) {
			char c = cl[i];
			if (c < '0' || c > '9') {
				ok = false; break;
			}
			need = need * 10 + static_cast<size_t>(c - '0');
			// TODO (пізніше): перевірити верхню межу й ліміти
		}
		if (!ok) {
			_state = REQUEST_ERROR;	return false;
		}
		if (need == 0) {
			_state = REQUEST_COMPLETE; return false;
		} else {
			_state = READING_FIXED_BODY;
			_expectedBodyLen = need;
			return true;
		}
	}
	//No body-defining headers found
	_state = REQUEST_COMPLETE; return false;
}

bool	HttpContext::chunkedBodyStateMachine(std::string &buf) {
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
		HttpParser::appendToBody(buf, _chunkSize, request());
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
 * REQUEST PARSER STATE MACHINE. It processes the _request_buffer
 * and transitions the client's state. It will loop as long as it can
 * make progress (e.g., parsing both request line and headers if they
 * are both in the buffer).
 */
void	HttpContext::requestParsingStateMachine() {
	
	string	&buf = connection().getBuffer();
	bool		can_parse = true;

	while (can_parse) {
		switch (_state) {
			case REQUEST_LINE: {
				size_t	pos = buf.find("\r\n");
				if (pos == string::npos) {
					can_parse = false; break;
				}
				string	line = buf.substr(0, pos);
				buf.erase(0, pos + 2);
				if (HttpParser::parseRequestLine(line, request()) == true) {
					//PrintUtils::printRequestLineInfo(request());
					_state = READING_HEADERS;
					continue;
				} else {
					_state = REQUEST_ERROR;
					can_parse = false; break;
				}
			}
			case READING_HEADERS : {
				if (!findAndParseHeaders(buf)) {
					can_parse = false; break;
				}
				//PrintUtils::printRequestHeaders(request());
				// TODO: Check for Content-Length or Transfer-Encoding
				if (isBodyToRead()) {
					//cout << YELLOW << "requestParsingStateMachine: we have a body to read" << RESET << endl;
					continue;
				} else {
					//cout << YELLOW << "requestParsingStateMachine: no body to read" << RESET << endl;
					can_parse = false; break;
				}
			}
			case READING_FIXED_BODY : {

				const size_t	bodySize = request().getBody().size();
				size_t			remaining;
				if (_expectedBodyLen > bodySize) {
					remaining = _expectedBodyLen - bodySize;
				} else {
					remaining = 0;
				}
				if (remaining == 0) {
					_state = REQUEST_COMPLETE;
					can_parse = false; break;
				}
				const size_t take = std::min(remaining, buf.size());
				if (take == 0) { // need more data from socket
					can_parse = false; break;
				}

				HttpParser::appendToBody(buf, take, request());
				buf.erase(0, take);

				if (request().getBody().size() == _expectedBodyLen) {
					HttpParser::parseFixBody(request().getBody(), request());
					_state = REQUEST_COMPLETE;
					can_parse = false;
				}
				break;
			}
			case READING_CHUNKED_BODY : {
				while (chunkedBodyStateMachine(buf)) {
					// The body of the loop can be empty.
				}
				if (_state != REQUEST_COMPLETE && _state != REQUEST_ERROR) {
					can_parse = false; // Pause parsing if we're waiting for more data
				}
				break;
			}

			case REQUEST_COMPLETE : {
				// debuggin
				/* if (request().getBody().size() != 0)
					PrintUtils::printBody(request()); */
				can_parse = false;
				break;
			}
			case REQUEST_ERROR  : {
				can_parse = false;
				break;
			}
			default : {
				std::cerr << "HttpContext class message: Unknown state of requst parsing" << endl;
				_state = REQUEST_ERROR;
				can_parse = false;
				break;
			}
		}
	}
}

/**
 * Checks if the received request is complete.
 * This is a basic implementation. A robust one would also check Content-Length.
 * @return true if the HTTP headers are complete, false otherwise.
 */
bool	HttpContext::isRequestComplete() const {
	if (_state == REQUEST_COMPLETE)
		return true;
	else 
		return false;
}

bool	HttpContext::isRequestError() const {
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
}

string	HttpContext::getResponseString() {
	string	body = _response.getResponseBody();
	short		status_code = _response.getStatusCode();
	size_t		content_length = _response.getContentLength();
	
	std::ostringstream	oss;

	// 1. Status Line
	oss << "HTTP/1.1 " << status_code << " OK\r\n";

	// 2. Headers
	oss << "Content-Type: text/html\r\n";
	oss << "Content-Length: " << content_length << "\r\n";
	oss << "Connection: keep-alive\r\n"; // Optional

	 // Add custom headers (like Location for redirects)
	const std::map<string, string>& headers = response().getHeaders();
	for (std::map<string, string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
		oss << it->first << ": " << it->second << "\r\n";
	}

	// 3. Empty Line (End of headers)
	oss << "\r\n";
	oss << body;

	return oss.str();
}

HttpContext::e_parse_state	HttpContext::getParserState() const {
	return _state;
}

// disabled operator, it is in private
HttpContext& HttpContext::operator=(const HttpContext& other) {
	(void)other;
	return *this;
}
