#include "Client.hpp"

// Parametic constructor
// Maryna: small update for good practice
Client::Client(Server &server) :
	response(server),
	server_config(server),
	_parser(),
	_request(),
	_fd(-1),
	_state(REQUEST_LINE)
	{ }

// Copy constructor
Client::Client(const Client &other)
	: response(other.server_config),   // створюємо новий Response, прив’язаний до того ж Server
	  server_config(other.server_config),
	  _parser(other._parser),          // якщо HttpParser копійований; інакше прибрати
	  _request(other._request),
	  _fd(other._fd),
	  _client_address(other._client_address),
	  _request_buffer(other._request_buffer),
	  _state(other._state)
{
	// Якщо треба перенести готовий body/status з other.response:
	// response = other.response; (потрібен copy ctor у Response)
}

Client::~Client() { }

/**
 * Receives data from the client's socket and appends it to the internal request buffer.
 * @return The number of bytes received, 0 on connection close, -1 on error.
 */
ssize_t	Client::receiveData() {

	char	buf[40000];
	int		nbytes = recv(_fd, buf, sizeof(buf), 0);
	
	if (nbytes > 0) {
		_request_buffer.append(buf, nbytes);
	}
	// debugging
	std::cout << "pollserver: recv " << nbytes << " bytes from fd " << getFd() << std::endl;
	if (nbytes > 0) {
		//We got some good data from a client (broadcast to other clients)
		std::cout << "message from " << getFd() << ":\n";
		int	to_print;
		if (nbytes < 100) to_print = nbytes;
		else to_print = 100;
		std::cout.write(buf, to_print);
		std::cout << std::endl;
	}
	
	return nbytes;
}

/**
 * REQUEST PARSER STATE MACHINE. It processes the _request_buffer
 * and transitions the client's state. It will loop as long as it can
 * make progress (e.g., parsing both request line and headers if they
 * are both in the buffer).
 */
void    Client::parseRequest() {

	//std::cout << "parseRequest " << std::endl;
	if (_request_buffer.size() > HttpParser::MAX_BUFFER_TOTAL) {
		_state = REQUEST_ERROR;
		_request_buffer.clear();
		std::cerr << "_request_buffer is larger than MAX_BUFFER_TOTAL";
		std::cerr << std::endl;
		return;
	}
	
	bool    can_parse = true;

	while (can_parse) {
		switch (_state) {
			case REQUEST_LINE: {
				size_t	pos = _request_buffer.find("\r\n");
				if (pos != std::string::npos) {
					parseRequestLine(_request_buffer.substr(0, pos));
					_request_buffer.erase(0, pos + 2);
					if (request().getRequestLineFormatValid())
						_state = READING_HEADERS;
					else
						_state = REQUEST_ERROR;
				} else {
					if (_request_buffer.size() > HttpParser::MAX_REQUEST_LINE) {
						_state = REQUEST_ERROR;
						_request_buffer.clear();
					}
					can_parse = false;
				}
				break ;
			}
			case READING_HEADERS : {
				size_t pos = _request_buffer.find("\r\n\r\n");
				if (pos != std::string::npos) {
					// TODO: Parse all headers from the header block
					parseHeaders(_request_buffer.substr(0, pos));
					// Remove headers from the buffer
					_request_buffer.erase(0, pos + 4);

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
					if (_request_buffer.size() > HttpParser::MAX_HEADER_BLOCK) {
						_state = REQUEST_ERROR;
						_request_buffer.clear();
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
					_request_buffer.clear();
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
				if (_request_buffer.size() >= static_cast<size_t>(need)) {
					parseBody(_request_buffer.substr(0, need));
					_request_buffer.erase(0, need);
					_state = REQUEST_COMPLETE;
				} else { // body not fully received
					if (_request_buffer.size() > HttpParser::MAX_BUFFER_TOTAL) {
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
				_request_buffer.clear();
				can_parse = false;
				break;
			}
			case REQUEST_ERROR  : {
				_request_buffer.clear();
				can_parse = false;
				break;
			}
			default : {
				std::cerr << "Client class message: Unknown state of requst parsing" << std::endl;
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
void	Client::parseRequestLine(std::string request_line) {

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

void	Client::parseHeaders(std::string headers) {
	//std::cout << "Parsing request headers" << std::endl;
	(void)headers;
	//std::cout << headers.substr(0, 20) << std::endl;
}

void	Client::parseBody(std::string body) {
	//std::cout << "Parsing request body" << std::endl;
	(void)body;
	//std::cout << body.substr(0, 20) << std::endl;
}

/**
 * Checks if the received request is complete.
 * This is a basic implementation. A robust one would also check Content-Length.
 * @return true if the HTTP headers are complete, false otherwise.
 */
bool	Client::isRequestComplete() const {
	if (_state == REQUEST_COMPLETE)
		return true;
	else 
		return false;
}

bool	Client::isRequestError() const {
	if (_state == REQUEST_ERROR)
		return true;
	else
		return false;
}

void	Client::resetState() {
	_state = REQUEST_LINE;
}

std::string	Client::getResponseString() {
	std::string body = response.getResponseBody();
	short status_code = response.getStatusCode();
	size_t content_length = response.getContentLength();
	std::string reason_phrase = response.getReasonPhrase();
	
	std::ostringstream oss;

	// 1. Status Line
	oss << "HTTP/1.1 " << status_code << " " << reason_phrase << "\r\n";

	// 2. Headers
	oss << "Content-Type: text/html\r\n";
	oss << "Content-Length: " << content_length << "\r\n";
	oss << "Connection: keep-alive\r\n"; // Optional

    // Add custom headers (like Location for redirects)
    const std::map<std::string, std::string>& headers = response.getHeaders();
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        oss << it->first << ": " << it->second << "\r\n";
    }

	// 3. Empty Line (End of headers)
	oss << "\r\n";
	oss << body;

	return oss.str();
}

HttpParser&	Client::parser() {
	return _parser;
}

Request&	Client::request() {
	return _request;
}

void	Client::setFd(int fd) { _fd = fd; }

int		Client::getFd() const { return _fd; }

void	Client::setClientAddress(const sockaddr_in& client_address) {
	_client_address = client_address;
}

std::string &	Client::getBuffer() {
	return _request_buffer;
}

Client::e_parse_state	Client::getParserState() const {
	return _state;
}

// disabled operator, it is in private
Client& Client::operator=(const Client& other) {
	(void)other;
	return *this;
}
