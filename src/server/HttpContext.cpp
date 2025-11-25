#include "HttpContext.hpp"

// Parametic constructor
HttpContext::HttpContext(Connection& conn, Server& server) :
	_conn(conn),
	_server_config(server),
	_request(),
	_response(server),
	_state(REQUEST_LINE)
	{ }

/* // Copy constructor
HttpContext::HttpContext(const HttpContext &other) :
	_response(other._server_config),
	_server_config(other._server_config),
	_request(other._request),
	_fd(other._fd),
	_client_address(other._client_address),
	_request_buffer(other._request_buffer),
	_state(other._state)
{ } */

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
void    HttpContext::parseRequest() {

	if (HttpParser::parseHeaderLine(connection().getBuffer(), request()) == true)
		std::cout << "<<<< _____HttpParser::parseHeaderLine()_____ >>>>" 
			<< std::endl;

	//std::cout << "parseRequest " << std::endl;
	if (connection().getBuffer().size() > HttpParser::MAX_BUFFER_TOTAL) {
		_state = REQUEST_ERROR;
		connection().getBuffer().clear();
		std::cerr << "_request_buffer is larger than MAX_BUFFER_TOTAL";
		std::cerr << std::endl;
		return;
	}
	
	bool    can_parse = true;

	while (can_parse) {
		switch (_state) {
			case REQUEST_LINE: {
				size_t	pos =connection().getBuffer().find("\r\n");
				if (pos != std::string::npos) {
					parseRequestLine(connection().getBuffer().substr(0, pos));
					connection().getBuffer().erase(0, pos + 2);
					if (request().getRequestLineFormatValid())
						_state = READING_HEADERS;
					else
						_state = REQUEST_ERROR;
				} else {
					if (connection().getBuffer().size() > HttpParser::MAX_REQUEST_LINE) {
						_state = REQUEST_ERROR;
						connection().getBuffer().clear();
					}
					can_parse = false;
				}
				break ;
			}
			case READING_HEADERS : {
				size_t pos = connection().getBuffer().find("\r\n\r\n");
				if (pos != std::string::npos) {
					// TODO: Parse all headers from the header block
					parseHeaders(connection().getBuffer().substr(0, pos));
					// Remove headers from the buffer
					connection().getBuffer().erase(0, pos + 4);

					// // TODO: Check for Content-Length or Transfer-Encoding
					// First step(it will be complicated):
					// check for Content-Length
					const std::string&	cl = request().getHeaderValue("Content-Length");
					if (cl.empty()) {
						//std::cout << "Request. Content-Length is missing.\n";
						//std::cout << "REQUEST_COMPLETE" << std::endl;
						_state = REQUEST_COMPLETE;
						can_parse = false;
					} else { // If a body is expected:
						_state = READING_BODY;
					}
				} else {
					if (connection().getBuffer().size() > HttpParser::MAX_HEADER_BLOCK) {
						_state = REQUEST_ERROR;
						connection().getBuffer().clear();
					}
					can_parse = false;
				}
				break ;
			}
			case READING_BODY : {
				// If using Content-Length:
				//   - Check if client.request_buffer.size() >= content_length
				//   - If yes: request is complete. client.state = REQUEST_COMPLETE; request_is_ready = true;
				//   - request.parseBody()
				//   - Remove body from the buffer
				const std::string& cl = request().getHeaderValue("Content-Length");
				if (cl.empty()) {
					_state = REQUEST_ERROR;
					connection().getBuffer().clear();
					can_parse = false;
					break;
				}

				long need = 0;
				// Reject non-digit characters early
				for (size_t di = 0; di < cl.size(); ++di) {
					if (cl[di] < '0' || cl[di] > '9') {
						_state = REQUEST_ERROR;
						need = -1;
						break;
					}
				}
				// Manual accumulation
				for (size_t di = 0; di < cl.size(); ++di) {
					need = need * 10 + (cl[di] - '0');
					if (need > (long)HttpParser::MAX_BUFFER_TOTAL) {
						_state = REQUEST_ERROR;
						break;
					}
				}
				if (connection().getBuffer().size() >= static_cast<size_t>(need)) {
					parseBody(connection().getBuffer().substr(0, need));
					connection().getBuffer().erase(0, need);
					_state = REQUEST_COMPLETE;
				} else { // body not fully received
					if (connection().getBuffer().size() > HttpParser::MAX_BUFFER_TOTAL) {
						std::cerr << "Request is larger than MAX_BUFFER_TOTAL\n";
						std::cerr << std::endl;
						_state = REQUEST_ERROR;
					}
					can_parse = false;
				}
				// If using chunked encoding:
				//   - Parse chunks until the final "0\r\n\r\n" is found.
				//   - request.parseBody()
				//   - Remove body from the buffer
				//   - If final chunk found: client.state = REQUEST_COMPLETE; request_is_ready = true;
				break ;
			}
			case REQUEST_COMPLETE : {
				connection().getBuffer().clear();
				can_parse = false;
				break;
			}
			case REQUEST_ERROR  : {
				connection().getBuffer().clear();
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

/**
 * Split by spaces into method, uri, version using string streams
 * 
 * Bind iss to the request_line string.
 */
void	HttpContext::parseRequestLine(std::string request_line) {

	if (request_line.empty())
	{
		this->request().setRequestLineFormatValid(false);
		this->request().setMethod("");
		return ;
	}

	std::istringstream			iss(request_line); 
	std::string					method, uri, version;

	if (!(iss >> method >> uri >> version) || !iss.eof()) {
		request().setRequestLineFormatValid(false);
		request().setMethod("");
		return;
	}
	request().setRequestLineFormatValid(true);

	if (method != "GET" && method != "POST" && method != "DELETE") {
		request().setRequestLineFormatValid(false);
		request().setMethod("");
	}

	if (version != "HTTP/1.1" && version != "HTTP/1.0") {
		request().setRequestLineFormatValid(false);
	}

	// Rudimentary URI check
	if (uri.empty() || uri[0] != '/' || uri.find("..") != std::string::npos) {
		request().setRequestLineFormatValid(false);
	}

	request().setMethod(method);
	request().setUri(uri);
	request().setVersion(version);

	//std::cout << "Request Line is parsed" << std::endl;
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
    _response = Response(server());
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
