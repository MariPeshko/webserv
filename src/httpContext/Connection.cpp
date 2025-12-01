#include "Connection.hpp"

Connection::Connection()
{ }

Connection::~Connection() { }

void	Connection::setFd(int fd) { _fd = fd; }

int		Connection::getFd() const { return _fd; }

void	Connection::setClientAddress(const sockaddr_in& client_address) {
	_client_address = client_address;
}

std::string &	Connection::getBuffer() {
	return _request_buffer;
}

/**
 * Receives data from the client's socket and appends it to the internal request buffer.
 * @return The number of bytes received, 0 on connection close, -1 on error.
 */
ssize_t			Connection::receiveData() {

	char	buf[40000];
	int		nbytes = recv(_fd, buf, sizeof(buf), 0);
	
	if (nbytes > 0) {
		_request_buffer.append(buf, nbytes);
	}
	// debugging
	std::cout << "server: recv " << nbytes << " bytes from fd " << getFd() << std::endl;
	if (nbytes > 0) {
		//We got some good data from a client (broadcast to other clients)
		std::cout << YELLOW << "message from " << getFd() << ":\n" << RESET;
		int	to_print;
		if (nbytes < 100) to_print = nbytes;
		else to_print = 100;
		std::cout.write(buf, to_print);
		if (to_print >= 100)
			std::cout << "\n(...message is limited to 100 bytes..))\n";
		std::cout << std::endl;
	}
	
	return nbytes;
}