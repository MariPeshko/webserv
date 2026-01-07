#include "ServerManager.hpp"

using std::string;
using std::map;
using std::vector;
using std::cout;
using std::endl;

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

/**
 * After each server's listening socket is successfully created, 
 * it is immediately registered for polling.
 */
void		ServerManager::setupServers(vector<Server> & server_configs ) {
	//_servers = server_configs;
	for (vector<Server>::iterator it = server_configs.begin(); 
			it != server_configs.end(); it++) {
		//socket setup
		//bind socket
		if (it->setupServer() == -1) {
			// You can add error handling logic here. For example, stop 
			// all servers from running or just ignore this one.
			std::cerr << "Error setting up a server. Skipping it." << endl;
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
				Logger::logErrno(LOG_ERROR, "Poll error");
				continue;
			}
			Logger::logErrno(LOG_ERROR, "Poll error");
			break;
		}
		if (poll_count > 0)
			processConnections();
		checkTimeouts();
	}
	cleanup();
	Logger::log(LOG_INFO, "Webserv stopped");
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

/** Handle incoming connections. IPv4 only
 * newfd - Newly accept()ed socket descriptor
 * 
 * - accept() fills remoteaddr with actual client address
 * - Make the accepted socket non-blocking
 * - Create Connection
 * - Create HttpContext bound to this connection+server
*/
void	ServerManager::handleNewConnection(int listener) {
	struct sockaddr_in		remoteaddr;
	socklen_t				addrlen = sizeof(remoteaddr);;
	int						newfd;
	
	
	newfd = accept(listener, reinterpret_cast<struct sockaddr*>(&remoteaddr), &addrlen);
	//                       ^^^^^^^^^^^^ Cast to base type for API
	if (newfd == -1) {
		Logger::logErrno(LOG_ERROR, "Accept failed");
		return;
	}
	int	flags = fcntl(newfd, F_GETFL, 0);
	if (flags != -1)
		fcntl(newfd, F_SETFL, flags | O_NONBLOCK);

	addToPfds(_pfds, newfd);
	
	Connection	conn;
	conn.setFd(newfd);
	conn.setClientAddress(remoteaddr);

	Server*		server = _map_servers[listener];
	
	_contexts.insert(std::make_pair(newfd, HttpContext(conn, *server)));

	uint32_t	ip = ntohl(remoteaddr.sin_addr.s_addr);
	std::string	ip_str = ipv4_to_string(ip);
	uint16_t	port = ntohs(remoteaddr.sin_port);
		
	Logger::log(LOG_INFO, "New connection accepted from " + ip_str + ":" + toString(port) + " by listener " + toString(newfd));
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
	const int	fd = _pfds[i].fd;

	// Find HttpContext
	map<int, HttpContext>::iterator	it = _contexts.find(fd);
	if (it == _contexts.end()) {
		Logger::logErrno(LOG_ERROR, "No context found for fd " + toString(fd));
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
		ctx.response().bindRequest(ctx.request());
		if (ctx.isRequestError()) {
			ctx.response().badRequest();
		} else {
			ctx.response().generateResponse();
		}
		ctx.buildResponseString();
		_pfds[i].events |= POLLOUT;
		Logger::logRequest(
			ipv4_to_string(ntohl(ctx.connection().getClientAddress().sin_addr.s_addr)),
			ctx.request().getMethod(),
			ctx.request().getUri(),
			ctx.response().getStatusCode(),
			ctx.response().getContentLength()
		);
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
 * 	 - If yes and it's "keep-alive" connection: reset state for the next 
 * 	   request and wait for POLLIN
 *   - Otherwise, response isn't complete - wait for next POLLOUT event
*/
void	ServerManager::handleClientWrite(size_t i) {
	const int								fd = _pfds[i].fd;
	map<int, HttpContext>::iterator	it = _contexts.find(fd);
	if (it == _contexts.end()) {
		Logger::logErrno(LOG_ERROR, "No context found for fd " + toString(fd));
		close(fd);
		delFromPfds(i);
		return;
	}
	HttpContext&		ctx = it->second;

	const string&	buffer = ctx.getResponseBuffer();
	size_t			already_sent = ctx.getBytesSent();
	
	if (buffer.size() <= already_sent) { // A safeguard
		_pfds[i].events = POLLIN;
		ctx.resetState();
		return;
	}
	size_t		remaining = buffer.size() - already_sent;
	const char*	data_ptr = buffer.c_str() + already_sent;
	ssize_t		bytes_sent = send(fd, data_ptr, remaining, 0);

	if (bytes_sent < 0) {
		Logger::logErrno(LOG_ERROR, "Send error on socket " + toString(fd));
		removeClient(fd, i);
		return;
	}
	if (bytes_sent == 0) {
		Logger::log(LOG_INFO, "Connection closed by peer on socket " + toString(fd));
		removeClient(fd, i);
		return;
	}
	ctx.connection().updateLastActivity();
	ctx.addBytesSent(static_cast<size_t>(bytes_sent));
	if (ctx.isResponseComplete()) {
		 if (ctx.request().getHeaderValue("connection") == "close") {
			Logger::log(LOG_INFO, "Connection: close. Closing socket " + toString(fd));
			removeClient(fd, i);
		} else {
			_pfds[i].events = POLLIN;
			ctx.resetState();
		}
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

void	ServerManager::handleErrorRevent(int fd, size_t i) {
	
	Logger::logErrno(LOG_ERROR, "Poll error on socket " + toString(fd));
	removeClient(fd,i);
}

/** Client socket error. Error message and cleanup */
void	ServerManager::handleClientError(int fd, size_t i) {
	Logger::logErrno(LOG_ERROR, "Socket error on fd " + toString(fd));
	removeClient(fd,i);
	
}

/** Client socket is closed. Error message and cleanup */
void	ServerManager::handleClientHungup(int fd, size_t i) {
	Logger::log(LOG_WARNING, "Socket " + toString(fd) + " hung up");
	removeClient(fd,i);
}

/** Remove a file descriptor at a given index from the vector. */
void	ServerManager::delFromPfds(size_t index) {
	if (index < _pfds.size()) {
		_pfds.erase(_pfds.begin() + index);
	}
}

/** close/erase logic */
void	ServerManager::removeClient(int fd, size_t i) {
	close(fd);
	_contexts.erase(fd);
	delFromPfds(i);
}

void	ServerManager::cleanup() {
	cout << "Closing all connections..." << endl;
	for (size_t i = 0; i < _pfds.size(); ++i) {
		if (close(_pfds[i].fd) == -1) {
			// TO DO to logger
			std::cerr << RED << "Error closing fd " << _pfds[i].fd << ": ";
			std::cerr << strerror(errno) << endl;
		}
	}
	// TO DO to logger
	cout << "Cleared " << _pfds.size() << " pfds and " << _contexts.size();
	cout << " contexts" << endl;
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
	time_t	timeout = 30; // 30 seconds timeout
	for (size_t i = 0; i < _pfds.size(); ) {
		int	fd = _pfds[i].fd;
		if (!isListener(fd)) {
			map<int, HttpContext>::iterator it = _contexts.find(fd);
			if (it != _contexts.end()) {
				 if (it->second.connection().hasTimedOut(timeout)) {
					//TO DO - add it to logger
					 cout << "Connection timed out on socket " << fd << endl;
					 removeClient(fd, i);
					 continue; 
				 }
			}
		}
		i++;
	}
}
