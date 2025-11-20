#include "Client.hpp"

Client::Client() : _fd(-1) {
}

Client::~Client() {
}

Client::Client(const Client &other) {
    _fd = other._fd;
    _client_address = other._client_address;
}

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
	//We got some good data from a client (broadcast to other clients)
	std::cout << "message from " << getFd() << ": ";
	std::cout.write(buf, 100);
	std::cout << std::endl;
	return nbytes;
}

/**
 * REQUEST PARSER STATE MACHINE. It processes the _request_buffer
 * and transitions the client's state. It will loop as long as it can
 * make progress (e.g., parsing both request line and headers if they
 * are both in the buffer).
 */
void    Client::parseRequest() {
    bool    can_parse = true;

    while (can_parse) {
		switch (_state) {
			case REQUEST_LINE: {
				size_t pos = _request_buffer.find("\r\n");
				if (pos != std::string::npos) {
					parseRequestLine(_request_buffer.substr(0, pos));
					_request_buffer.erase(0, pos + 2);
					_state = READING_HEADERS;
				} else {
					can_parse = false;
				}
				break ;
			}
			case READING_HEADERS : {
				size_t pos = _request_buffer.find("\r\n\r\n");
				if (pos != std::string::npos) {
					// TODO: Parse all headers from the header block
					// For example: request.parseHeaders(_request_buffer.substr(0, pos));
                    // Remove headers from the buffer
                    _request_buffer.erase(0, pos + 4);

					// TODO: Check for Content-Length or Transfer-Encoding
                    // If a body is expected:
                    //   _state = READING_BODY;
                    // Else (no body):
					_state = REQUEST_COMPLETE;
				} else {
					can_parse = false;
				}
				break ;
			}
			case READING_BODY : {
				_state = REQUEST_COMPLETE;
				break ;
			}
			case REQUEST_COMPLETE :
			case REQUEST_ERROR  : {
				can_parse = false;
                break;
			}
		}
    }
}

// Split raw_request by spaces into method, uri, version using string streams
void	Client::parseRequestLine(std::string request_line) {

    if (request_line.size() == 0) return;

	std::string			line(request_line);
	std::istringstream	iss(line);
	std::string			method, uri, version;
	if (iss >> method >> uri >> version) {
		this->request.setMethod(method);
		this->request.setUri(uri);
		this->request.setVersion(version);
	}
}

/**
 * Checks if the received request is complete.
 * This is a basic implementation. A robust one would also check Content-Length.
 * @return true if the HTTP headers are complete, false otherwise.
 */
bool	Client::isRequestComplete() {
	if (_state == REQUEST_COMPLETE)
		return true;
	else 
		return false;
}

void	Client::resetState() {
	_state = REQUEST_LINE;
}

std::string	Client::getResponseString() {
    std::string body = response.getResponseBody();
    std::string status_code = response.getStatusCode();
    size_t content_length = response.getContentLength();
    
    std::ostringstream oss;

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

void	Client::setFd(int fd) { _fd = fd; }

int		Client::getFd() { return _fd; }

void	Client::setClientAddress(const sockaddr_in& client_address) {
    _client_address = client_address;
}

std::string &	Client::getBuffer() {
	return _request_buffer;
}
