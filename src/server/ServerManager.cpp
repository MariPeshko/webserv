#include "ServerManager.hpp"

ServerManager::ServerManager() {
}

ServerManager::~ServerManager() {
}


/**
 * After each server's listening socket is successfully created, 
 * it is immediately registered for polling.
 */
void ServerManager::setupServers(std::vector<Server> & server_configs ) {
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
 */
void	ServerManager::runServers() {
	while (1) { // Later: signal handling
		int	poll_count = poll(&_pfds[0], _pfds.size(), 1000);  // 1 second maximum time to wait
		
 if (poll_count == -1) {
			// if (!g_running) break;  // Signal received
			Logger::logErrno(LOG_ERROR, "Poll error");
			break;
		}
		if (poll_count > 0) {
			processConnections();
		}
	}
	//Loop is broken. Cleanup
	std::cout << "Closing all connections..." << std::endl;
	for (size_t i = 0; i < _pfds.size(); ++i) {
		close(_pfds[i].fd);
	}
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

/** Handle incoming connections. */
void	ServerManager::handleNewConnection(int listener) {
	struct sockaddr_in		remoteaddr;  // IPv4 only
	socklen_t				addrlen = sizeof(remoteaddr);;
	int						newfd;		// Newly accept()ed socket descriptor
	
	// accept() fills remoteaddr with actual client address
	newfd = accept(listener, reinterpret_cast<struct sockaddr*>(&remoteaddr), &addrlen);
	//                       ^^^^^^^^^^^^ Cast to base type for API

	if (newfd == -1) {
		Logger::logErrno(LOG_ERROR, "Accept failed");
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
	std::string ip_str = toString(ip);
	//uint16_t port = ntohs(remoteaddr.sin_port);
		
	Logger::log(LOG_INFO, "New connection accepted from " + ip_str);
	
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
		Logger::logRequest(
			toString(ntohl(ctx.connection().getClientAddress().sin_addr.s_addr)),
			ctx.request().getMethod(),
			ctx.request().getUri(),
			ctx.response().getStatusCode(),
			ctx.response().getContentLength()
		);
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

/** Handle POLLOUT event - send response data when socket is ready */
void	ServerManager::handleClientWrite(size_t i) {
	const int								fd = _pfds[i].fd;
	std::map<int, HttpContext>::iterator	it = _contexts.find(fd);
	if (it == _contexts.end()) {
		Logger::logErrno(LOG_ERROR, "No context found for fd " + toString(fd));
		close(fd);
		delFromPfds(i);
		return;
	}
	HttpContext&		ctx = it->second;
	// Get remaining data to send
	const std::string&	buffer = ctx.getResponseBuffer();
	size_t				already_sent = ctx.getBytesSent();
	
	if (buffer.size() <= already_sent) { // a safeguard
		// Response fully sent, switch off POLLOUT
		_pfds[i].events = POLLIN;
		ctx.resetState();
		return;
	}
	// Send remaining data
	size_t		remaining = buffer.size() - already_sent;
	const char*	data_ptr = buffer.c_str() + already_sent;
	ssize_t		bytes_sent = send(fd, data_ptr, remaining, 0); /// ? last parameter NON_BLOCK

	if (bytes_sent == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			// Socket buffer full, wait for next POLLOUT
			return;
		}
		// Real error occurred
		Logger::logErrno(LOG_ERROR, "Send error on socket " + toString(fd));
		removeClient(fd, i);
		return;
	}
	if (bytes_sent == 0) {
		// Connection closed by peer
		Logger::log(LOG_INFO, "Connection closed by peer on socket " + toString(fd));
		removeClient(fd, i);
		return;
	}
	// Update bytes sent counter
	ctx.addBytesSent(static_cast<size_t>(bytes_sent));
	// Check if response is complete
	if (ctx.isResponseComplete()) {
		 if (ctx.request().getHeaderValue("connection") == "close") {
            Logger::log(LOG_INFO, "Connection: close. Closing socket " + toString(fd));
            removeClient(fd, i);
        } else {
            // Keep-alive: reset state for the next request and wait for POLLIN
            _pfds[i].events = POLLIN;
            ctx.resetState();
        }
	}
	// Otherwise, wait for next POLLOUT event
}


