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

const sockaddr_in &Connection::getClientAddress() const
{
	return _client_address;
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
	if (CON_DEBUG) std::cout << YELLOW << "server: recv " << nbytes << " bytes from fd " << getFd() << ". ";
	if (nbytes > 0) {
		if (CON_DEBUG) std::cout << "message:\n" << RESET;
		int	to_print;
		if (nbytes < 100) to_print = nbytes;
		else to_print = 100;
		if (CON_DEBUG) std::cout.write(buf, to_print);
		if (CON_DEBUG >= 100)
			if (CON_DEBUG) std::cout << "\n(...message is limited to 100 bytes..))\n";
		if (CON_DEBUG) std::cout << std::endl;
	}
	
	return nbytes;
}