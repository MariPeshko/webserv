#include "ServerManager.hpp"

ServerManager::ServerManager() {
}

ServerManager::~ServerManager() {
}

/**
 * After each server's listening socket is successfully created, 
 * it is immediately registered for polling.
 */
void ServerManager::setupServers(std::vector<Server> server_configs) {
    _servers = server_configs;
    for (std::vector<Server>::iterator it = _servers.begin(); it != _servers.end(); it++) {
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

/** Convert IPv4 address to string manually (C++98 compatible) */
std::string ipv4_to_string(uint32_t ip) {
    std::ostringstream oss;
    oss << ((ip >> 24) & 0xFF) << "."
        << ((ip >> 16) & 0xFF) << "."
        << ((ip >> 8) & 0xFF) << "."
        << (ip & 0xFF);
    return oss.str();
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
    } else {
		Client	newClient;

		addToPfds(_pfds, newfd);
		newClient.setFd(newfd);
		// Remove previous client if exists
		if (_clients.count(newfd) != 0) {
			_clients.erase(newfd);
		}
		_clients[newfd] = newClient;

		uint32_t ip = ntohl(remoteaddr.sin_addr.s_addr);
		std::string ip_str = ipv4_to_string(ip);
		uint16_t port = ntohs(remoteaddr.sin_port);
		
		std::cout << "pollserver: new connection from " << ip_str << ":" << port
				<< " on socket " << newfd << " (accepted by listener " << listener << ")" << std::endl;
    }
}

void	ServerManager::sendResponse(Client& client) {
	std::string response = client.getResponseString();

	send(client.getFd(), response.c_str(), response.size(), 0);
}

void	ServerManager::handleClientError(Client& client, size_t i) {
	std::cout << "pollserver: socket " << client.getFd() << " error after recv()" << std::endl;
	close(client.getFd());
	_clients.erase(client.getFd());
     delFromPfds(i);
}

void	ServerManager::handleClientHungup(Client& client, size_t i) {
	std::cout << "pollserver: socket " << client.getFd() << " hung up" << std::endl;
	close(client.getFd());
	_clients.erase(client.getFd());
    delFromPfds(i);
}

/**
 * In case of POLLHUP client hung up (disconnected) and
 * recv() returns 0 (EOF).
 * 
 * Reads the available chunk of data (which might be an incomplete 
 * request). Stores that chunk in the corresponding Client object's 
 * internal buffer.
 */
void	ServerManager::handleClientData(size_t i) {
	
	std::map<int, Client>::iterator it = _clients.find(_pfds[i].fd);
	if (it == _clients.end()) {
		std::cerr << "Error: No client found for fd " << _pfds[i].fd << std::endl;
		close(_pfds[i].fd);
		delFromPfds(i);
		return ;
	}
	Client&	client = it->second;

	ssize_t nbytes = client.receiveData();
	if (nbytes == 0) {
		handleClientHungup(client, i);
		return ;
	}
	if (nbytes < 0) {
		handleClientError(client, i);
		return ;
	}

	client.parseRequest();
	// Only parse and respond if the request is complete
    if (client.isRequestComplete()) {
		
        // The parseRequest method should now use the client's internal buffer
		client.response.generateResponse();
        sendResponse(client);
		// to do: clear buffer, reset client state to default
		client.resetState();
    }
    // If request is not complete, we do nothing and wait for the next poll() event.
}

void	ServerManager::handleErrorRevent(size_t i) {
	
	std::cerr << "Poll error on socket " << _pfds[i].fd << std::endl;
	close(_pfds[i].fd);
	_clients.erase(_pfds[i].fd);
	delFromPfds(i);
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
			handleErrorRevent(i);
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

/**
 *  poll() reports revent(s).
 */
void	ServerManager::runServers() {
	while (1) { // Later: signal handling
		// poll() returns the number of elements in the pfds
		// array for which events have occurred
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
	// Loop is broken. Cleanup
	// std::cout << "Closing all connections..." << std::endl;
	//for (size_t i = 0; i < pfds.size(); ++i) {
	//	close(pfds[i].fd);
    //}
	//std::cout << "Server shut down cleanly" << std::endl;
}

bool	ServerManager::isListener(int fd) {
	return _map_servers.find(fd) != _map_servers.end();
}
