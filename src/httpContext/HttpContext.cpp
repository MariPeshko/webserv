#include "HttpContext.hpp"

// Parametic constructor
HttpContext::HttpContext(Connection& conn, Server& server) :
	_conn(conn),
	_server_config(server),
	_request(),
	_response(server),
	_state(REQUEST_LINE),
	_expectedBodyLen(0)
	{ }

// Copy constructor
HttpContext::HttpContext(const HttpContext &other) :
	_conn(other._conn),
	_server_config(other._server_config),
	_request(other._request),
	_response(other._server_config),
	_state(other._state),
	_expectedBodyLen(other._expectedBodyLen)
{ }

HttpContext::~HttpContext() { }

Connection& HttpContext::connection() { return _conn; }
Server&     HttpContext::server()     { return _server_config; }
Request&    HttpContext::request()    { return _request; }
Response&   HttpContext::response()   { return _response; }

/**
 * REQUEST PARSER STATE MACHINE. It processes the _request_buffer
 * and transitions the client's state. It will loop as long as it can
 * make progress (e.g., parsing both request line and headers if they
 * are both in the buffer).
 */
void	HttpContext::parseRequest() {
	
	std::string	&buf = connection().getBuffer();
	bool		can_parse = true;

	while (can_parse) {
		switch (_state) {
			case REQUEST_LINE: {
				size_t	pos = buf.find("\r\n");
				if (pos == std::string::npos) {
					can_parse = false; break;
				}
				std::string	line = buf.substr(0, pos);
				buf.erase(0, pos + 2);
				if (HttpParser::parseRequestLine(line, request()) == true) {
					_state = READING_HEADERS;
					continue;
				} else {
					_state = REQUEST_ERROR;
					can_parse = false; break;
				}
			}
			case READING_HEADERS : {
				size_t pos = buf.find("\r\n\r\n");
				if (pos == std::string::npos) {
					can_parse = false; break;
				}
				// TODO: Parse all headers from the header block
				std::string	rawHeaders = buf.substr(0, pos);
				buf.erase(0, pos + 4);
				if (HttpParser::parseHeaders(rawHeaders, request()) == false) {
					_state = REQUEST_ERROR;
					can_parse = false; break;
				}

				const std::string& cl = request().getHeaderValue("Content-Length");
				if (cl.empty()) {
					_state = REQUEST_COMPLETE;
					can_parse = false; break;
				}

				// Reject non-digit characters early and Manual accumulation
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
					_state = REQUEST_ERROR;
					can_parse = false;
					break;
				}

				_expectedBodyLen = need;
				_state = READING_BODY;
				continue;

				// // TODO: Check for Content-Length or Transfer-Encoding
				// First step(it will be complicated):
				// check for Content-Length
			}
			case READING_BODY : {
				// If using Content-Length:
				//   - Check if client.request_buffer.size() >= content_length
				//   - If yes: request is complete. client.state = REQUEST_COMPLETE; request_is_ready = true;
				//   - request.parseBody()
				//   - Remove body from the buffer
				if (buf.size() < _expectedBodyLen) {
					can_parse = false;
					break;
				}
				std::string body = buf.substr(0, _expectedBodyLen);
				buf.erase(0, _expectedBodyLen);
				parseBody(body);
				_state = REQUEST_COMPLETE;
				can_parse = false;
				// If using chunked encoding:
				//   - Parse chunks until the final "0\r\n\r\n" is found.
				//   - request.parseBody()
				//   - Remove body from the buffer
				//   - If final chunk found: client.state = REQUEST_COMPLETE; request_is_ready = true;
				break ;
			}
			case REQUEST_COMPLETE :
			case REQUEST_ERROR  : {
				can_parse = false;
				break;
			}
			default : {
				std::cerr << "HttpContext class message: Unknown state of requst parsing" << std::endl;
				_state = REQUEST_ERROR;
				can_parse = false;
				break;
			}
		}
	}
}

void	HttpContext::parseHeaders(std::string headers) {
	//std::cout << "Parsing request headers" << std::endl;
	(void)headers;
	//std::cout << headers.substr(0, 20) << std::endl;
}

void	HttpContext::parseBody(std::string body) {
	//std::cout << "Parsing request body" << std::endl;
	(void)body;
	//std::cout << body.substr(0, 20) << std::endl;
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
}

std::string	HttpContext::getResponseString() {
	std::string	body = _response.getResponseBody();
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
    const std::map<std::string, std::string>& headers = response().getHeaders();
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
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
