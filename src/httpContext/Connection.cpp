#include "Connection.hpp"

Connection::Connection()
    : _last_activity(time(NULL))
{ }

Connection::~Connection() { }

/**
 * Update the timestamp of the last activity on this connection.
 * Called whenever data is received or sent to keep the timeout logic accurate.
 */
void    Connection::updateLastActivity() {
    _last_activity = time(NULL);
}

/**
 * Check whether this connection has been idle for more than `timeout` seconds.
 * Uses the last activity timestamp maintained by updateLastActivity().
 */
bool    Connection::hasTimedOut(time_t timeout) const {
    return (time(NULL) - _last_activity) > timeout;
}

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
 * Updates the last activity timestamp on each successful read.
 * @return The number of bytes received, 0 on connection close, -1 on error.
 */
ssize_t			Connection::receiveData() {

	char	buf[40000];
	int		nbytes = recv(_fd, buf, sizeof(buf), 0);
	
	updateLastActivity();

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