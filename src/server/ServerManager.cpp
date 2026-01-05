#include "ServerManager.hpp"

ServerManager::ServerManager() : shutdown(false) {}

ServerManager::~ServerManager() {}

// Checker for the main server loop
bool		ServerManager::isShutdownRequested() const {
	return shutdown;
}

// Signal-safe method to request shutdown, triggered by signals.
void		ServerManager::requestShutdown() {
	shutdown = true;
}

/** Convert IPv4 address to string manually (C++98 compatible) */
std::string	ipv4_to_string(uint32_t ip) {
	std::ostringstream oss;
	oss << ((ip >> 24) & 0xFF) << "."
		<< ((ip >> 16) & 0xFF) << "."
		<< ((ip >> 8) & 0xFF) << "."
		<< (ip & 0xFF);
	return oss.str();
}

/**
 * After each server's listening socket is successfully created, 
 * it is immediately registered for polling.
 */
void		ServerManager::setupServers(std::vector<Server> & server_configs ) {
	//_servers = server_configs;
	for (std::vector<Server>::iterator it = server_configs.begin(); it != server_configs.end(); it++) {
		//socket setup
		//bind socket
		if (it->setupServer() == -1) {
			// You can add error handling logic here.
			// For example, stop all servers from running or just ignore this one.
			std::cerr << "Error setting up a server. Skipping it." << std::endl;
		} else {
			// Add listener to set of pollfd-s
			addToPfds(_pfds, it->getListenFd());
			_map_servers[it->getListenFd()] = &(*it);
		}
	}
}

/**
 * poll() based single-thread loop is a requirement of the assignment.
 * 
 * poll() reports revent(s).
 * poll() returns the number of elements in the pfds array 
 * for which events have occurred.
 * 
 * isShutdownRequested() method checks for the incoming signals
 */
void	ServerManager::runServers() {
	while (!isShutdownRequested()) {
		int	poll_count = poll(&_pfds[0], _pfds.size(), 1000);  // 1 second maximum time to wait
		if (poll_count == -1) {
			if (errno == EINTR) { // a signal occurred
				// TO DO it shoud be in LOG
				std::cout << RED << "Signal interrupted poll()\n" << RESET;
				continue;
			}
			// TO DO it shoud be in LOG
			std::cerr << "Poll error: " << strerror(errno) << std::endl;
			break;
		}
		if (poll_count > 0)
			processConnections();
		checkTimeouts();
	}
	cleanup();
	std::cout << GREEN << "Server shuts down cleanly" << RESET << std::endl;
}

/** 
 * Process all existing connections
 * 
 * The revents field is a bitmask.
 * 
 * Check if there is error of if someone's ready to read or 
 * lient disconnected (or both).
 * 
 * It doesn't increment i, if the element was deleted from Pfds 
 * and map of clients. In this case, the next element is now at 
 * the current index.
*/
void	ServerManager::processConnections() {
	for (size_t i = 0; i < _pfds.size(); ) {
		
		if (_pfds[i].revents & POLLERR) {
			handleErrorRevent(_pfds[i].fd, i);
			continue;
		}
		if (_pfds[i].revents & (POLLIN | POLLHUP)) {
			size_t	pfd_size_before = _pfds.size();
			if (isListener(_pfds[i].fd)) {
				handleNewConnection(_pfds[i].fd);
			} else {
				handleClientData(i);
				if (_pfds.size() < pfd_size_before) {
					continue;
				}
			}
		}
		if (_pfds[i].revents & POLLOUT) {
			size_t	pfd_size_before = _pfds.size();
			handleClientWrite(i);
			if (_pfds.size() < pfd_size_before) {
				continue;
			}
		}
		i++;
	}
}

/** Handle incoming connections. */
void	ServerManager::handleNewConnection(int listener) {
	struct sockaddr_in		remoteaddr;  // IPv4 only
	socklen_t				addrlen = sizeof(remoteaddr);;
	int						newfd;		// Newly accept()ed socket descriptor
	
	// accept() fills remoteaddr with actual client address
	newfd = accept(listener, reinterpret_cast<struct sockaddr*>(&remoteaddr), &addrlen);
	//                       ^^^^^^^^^^^^ Cast to base type for API

	if (newfd == -1) {
		std::cerr << "Accept failed: " << strerror(errno) << std::endl;
		return;
	}

	// Make the accepted socket non-blocking
	int	flags = fcntl(newfd, F_GETFL, 0);
	if (flags != -1)
		fcntl(newfd, F_SETFL, flags | O_NONBLOCK);

	addToPfds(_pfds, newfd);
	
	// Create Connection
	Connection	conn;
	conn.setFd(newfd);
	conn.setClientAddress(remoteaddr);
	Server* server = _map_servers[listener];
	
	// Create HttpContext bound to this connection+server
	_contexts.insert(std::make_pair(newfd, HttpContext(conn, *server)));

	uint32_t ip = ntohl(remoteaddr.sin_addr.s_addr);
	std::string ip_str = ipv4_to_string(ip);
	uint16_t port = ntohs(remoteaddr.sin_port);
		
	std::cout << GREEN << "server: new connection ";
	std::cout << "from " << ip_str << ":" << port;
	std::cout << "\non socket " << newfd << " (accepted by listener ";
	std::cout << listener << ")" << RESET << std::endl;
}

void	ServerManager::handleErrorRevent(int fd, size_t i) {
	
	std::cerr << "Poll error on socket " << fd << std::endl;
	removeClient(fd,i);
}

/** Client socket error. Error message and cleanup */
void	ServerManager::handleClientError(int fd, size_t i) {
	std::cout << "server: socket " << fd << " error after recv()" << std::endl;
	removeClient(fd,i);
	
}

/** Client socket is closed. Error message and cleanup */
void	ServerManager::handleClientHungup(int fd, size_t i) {
	std::cout << "server: socket " << fd << " hung up" << std::endl;
	removeClient(fd,i);
}

/** close/erase logic */
void	ServerManager::removeClient(int fd, size_t i) {
	close(fd);
	_contexts.erase(fd);
	delFromPfds(i);
}

/**
 * In case of POLLHUP client hung up (disconnected) and
 * recv() returns 0 (EOF).
 * 
 * Reads the available chunk of data (which might be an incomplete 
 * request). Stores that chunk in the corresponding HttpContext object's 
 * internal buffer.
 * 
 * If request is not complete, we do nothing and wait for the next poll() event.
 */
void	ServerManager::handleClientData(size_t i) {
	const int fd = _pfds[i].fd;

	// Find HttpContext
	std::map<int, HttpContext>::iterator it = _contexts.find(fd);
	if (it == _contexts.end()) {
		std::cerr << "Error: No contexts found for fd " << fd << std::endl;
		close(fd);
		delFromPfds(i);
		return ;
	}
	HttpContext&	ctx = it->second;

	ssize_t nbytes = ctx.connection().receiveData();
	if (nbytes == 0) {
		handleClientHungup(fd, i);
		return ;
	}
	if (nbytes < 0) {
		handleClientError(fd, i);
		return ;
	}
	ctx.requestParsingStateMachine();
	if (ctx.isRequestComplete() || ctx.isRequestError()) {
		
		// Check if it is cgi
		// HttpContext method for checking if server has a /cgi-bin
		// If it's a CGI request: You trigger your CGI execution logic. 
		// If it's NOT a CGI request: You proceed with 
		// the normal static file handling logic
		ctx.response().bindRequest(ctx.request());
		if (ctx.isRequestError()) {
			ctx.response().badRequest();
		} else if (ctx.request().getEnumMethod() == Request::POST) {
			ctx.response().postAndGenerateResponse();
		} else if (ctx.request().getEnumMethod() == Request::DELETE) {
			ctx.response().deleteAndGenerateResponse();
		} else if (ctx.request().getEnumMethod() == Request::INVALID) {
			ctx.response().fillResponse(400, ctx.response().getErrorPageContent(400));
		} else {
			ctx.response().generateResponse();
		}
		ctx.buildResponseString();
		_pfds[i].events |= POLLOUT;
	}
}

bool	ServerManager::isListener(int fd) {
	return _map_servers.find(fd) != _map_servers.end();
}

// Adding both listeners and new clients to be monitored for reading.
// POLLIN - Check ready-to-read
void	ServerManager::addToPfds(std::vector<pollfd>& pfds, int newfd) {
	
	pollfd	pfd;

	pfd.fd = newfd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	
	pfds.push_back(pfd);
}

/** Remove a file descriptor at a given index from the vector. */
void	ServerManager::delFromPfds(size_t index) {
	if (index < _pfds.size()) {
		_pfds.erase(_pfds.begin() + index);
	}
}

/** Handle POLLOUT event - send response data when socket is ready 
 * 
 * - Get remaining data to send
 * - A safeguard: If response fully sent, switch off POLLOUT
 * - Send remaining data
 * - Check send() return value
 * - Updates the last activity timestamp on each successful send
 * - Updates bytes sent counter
 * - Check if response is complete
 *   - If yes and it's "close" connection - close it
 * 	 - If yes and it's "keep-alive" connection: reset state for the next request and wait for POLLIN
 *   - Otherwise, response isn't complete - wait for next POLLOUT event
*/
void	ServerManager::handleClientWrite(size_t i) {
	const int								fd = _pfds[i].fd;
	std::map<int, HttpContext>::iterator	it = _contexts.find(fd);
	if (it == _contexts.end()) {
		std::cerr << "Error: No context found for fd " << fd << std::endl;
		close(fd);
		delFromPfds(i);
		return;
	}
	HttpContext&		ctx = it->second;

	const std::string&	buffer = ctx.getResponseBuffer();
	size_t				already_sent = ctx.getBytesSent();
	
	if (buffer.size() <= already_sent) { // A safeguard
		_pfds[i].events = POLLIN;
		ctx.resetState();
		return;
	}
	size_t		remaining = buffer.size() - already_sent;
	const char*	data_ptr = buffer.c_str() + already_sent;
	ssize_t		bytes_sent = send(fd, data_ptr, remaining, 0);

	if (bytes_sent < 0) {
		// TO DO DELTE - this is forbidden by subject
		//if (errno == EAGAIN || errno == EWOULDBLOCK)
		// TO DO - add to logger
		std::cerr << "Send error on socket " << fd << ": " << strerror(errno) << std::endl;
		removeClient(fd, i);
		return;
	}
	if (bytes_sent == 0) {
		// // TO DO - add to logger: Connection closed by peer
		std::cout << "_pfds[i].events &= ~POLLOUT connection closed by peer on socket " << fd << std::endl;
		removeClient(fd, i);
		return;
	}
	ctx.connection().updateLastActivity();
	ctx.addBytesSent(static_cast<size_t>(bytes_sent));
	if (ctx.isResponseComplete()) {
		 if (ctx.request().getHeaderValue("connection") == "close") {
			std::cout << "Connection: close. Closing socket " << fd << std::endl;
			removeClient(fd, i);
		} else {
			_pfds[i].events = POLLIN;
			ctx.resetState();
		}
	}
}

void	ServerManager::cleanup() {
	std::cout << "Closing all connections..." << std::endl;
	for (size_t i = 0; i < _pfds.size(); ++i) {
		if (close(_pfds[i].fd) == -1) {
			// TO DO to logger
			std::cerr << RED << "Error closing fd " << _pfds[i].fd << ": " << strerror(errno) << std::endl;
		}
	}
	// TO DO to logger
	std::cout << "Cleared " << _pfds.size() << " pfds and " << _contexts.size() << " contexts" << std::endl;
	_pfds.clear();
	_contexts.clear();
}

/**
 * Scan all active client connections and close those that have been idle
 * longer than the configured inactivity timeout.
 *
 * This is mainly used to enforce a keep‑alive timeout: if a client keeps
 * the TCP connection open but does not send any data for `timeout` seconds,
 * the connection is considered stale and is closed to free resources.
 *
 * Algorithm:
 *  - Iterate over all fds in `_pfds`.
 *  - Skip listener sockets.
 *  - For each client fd, find its `HttpContext` and call
 *    `connection().hasTimedOut(timeout)`.
 *  - If the connection has timed out, log it and call `removeClient(fd, i)`.
 *    Do not increment `i` when an element is removed, because `_pfds` shrinks
 *    and the next element moves into the current index.
 */
void	ServerManager::checkTimeouts() {
	time_t	timeout = 10; // 10 seconds timeout
	for (size_t i = 0; i < _pfds.size(); ) {
		int	fd = _pfds[i].fd;
		if (!isListener(fd)) {
			std::map<int, HttpContext>::iterator it = _contexts.find(fd);
			if (it != _contexts.end()) {
				 if (it->second.connection().hasTimedOut(timeout)) {
					//TO DO - add it to logger
					 std::cout << "Connection timed out on socket " << fd << std::endl;
					 removeClient(fd, i);
					 continue; 
				 }
			}
		}
		i++;
	}
}
