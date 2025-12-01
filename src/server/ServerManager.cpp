#include "ServerManager.hpp"

ServerManager::ServerManager() {
}

ServerManager::~ServerManager() {
}

/** Convert IPv4 address to string manually (C++98 compatible) */
std::string ipv4_to_string(uint32_t ip) {
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
			std::cerr << "Poll error: " << strerror(errno) << std::endl;
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
	std::cout << "Server shut down cleanly" << std::endl;
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
	//uint16_t port = ntohs(remoteaddr.sin_port);
		
	std::cout << GREEN << "server: new connection";
	//from " << ip_str << ":" << port
	std::cout << " on socket " << newfd << " (accepted by listener ";
	std::cout << listener << ")" << RESET << std::endl;
}

void	ServerManager::sendResponse(int fd, HttpContext& ctx) {
	std::string response = ctx.getResponseString();

	ssize_t bytes_sent = send(fd, response.c_str(), response.size(), 0);
	
	if (bytes_sent == -1) {
		// This will now happen on a broken pipe instead of a crash
		std::cerr << "Send error on socket " << fd << std::endl;
		// You might want to trigger client cleanup here, though the main loop
		// will likely catch the POLLERR on the next iteration anyway.
	}

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
	// Only parse and respond if the request is complete
	if (ctx.isRequestComplete() || ctx.isRequestError()) {
		// Check if it is cgi
		// HttpContext method for checking if server
		// has a /cgi-bin
		// If it's a CGI request: You trigger your CGI execution logic. 
		// If it's NOT a CGI request: You proceed with 
		// the normal static file handling logic

		// The parseRequest method should now use the client's internal buffer
		// Generate response
		ctx.response().bindRequest(ctx.request());
		ctx.response().generateResponse();
		sendResponse(fd, ctx);
		ctx.resetState();
	}
	// If request is not complete, we do nothing and wait for the next poll() event.
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
