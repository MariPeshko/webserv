#include "Client.hpp"

Client::Client() {
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
 * Checks if the received request is complete.
 * This is a basic implementation. A robust one would also check Content-Length.
 * @return true if the HTTP headers are complete, false otherwise.
 */
bool	Client::isRequestComplete() {
    // A complete request has at least the double CRLF.
    // A more robust check is needed for requests with bodies (e.g., POST).
    return _request_buffer.find("\r\n\r\n") != std::string::npos;
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
