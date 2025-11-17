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
			add_to_pfds(_pfds, it->getListenFd());
			_map_servers[it->getListenFd()] = &(*it);
		}
	}
}

// Adding both listeners and new clients to be monitored for reading.
void	ServerManager::add_to_pfds(std::vector<pollfd>& pfds, int newfd) {
	
	pollfd	pfd;

	pfd.fd = newfd;
	pfd.events = POLLIN; // Check ready-to-read
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
        add_to_pfds(_pfds, newfd);

		uint32_t ip = ntohl(remoteaddr.sin_addr.s_addr);
		std::string ip_str = ipv4_to_string(ip);
		uint16_t port = ntohs(remoteaddr.sin_port);
		
		std::cout << "pollserver: new connection from " << ip_str << ":" << port
				<< " on socket " << newfd << " (accepted by listener " << listener << ")" << std::endl;
    }
}

void	ServerManager::handleClientData(size_t& i) {
	
	char	buf[40000];    // Buffer for client data

	// In case of POLLHUP - Client hung up (disconnected) recv() returns 0 (EOF)
	int		nbytes = recv(_pfds[i].fd, buf, sizeof(buf), 0);
	int		sender_fd = _pfds[i].fd;
	
	if (nbytes <= 0) { // Got error or connection closed by client
        if (nbytes == 0) {
            // Connection closed
            std::cout << "pollserver: socket " << sender_fd << " hung up" << std::endl;
        } else {
			std::cout << "pollserver: socket " << sender_fd << " error" << std::endl;  // ✅ No errno
            //std::cerr << "Recv error: " << strerror(errno) << std::endl;
        }
        close(_pfds[i].fd);
        delFromPfds(i);
        if (i > 0) --i; // reexamine the slot we just deleted
		return;
    }
	
	// We got some good data from a client (broadcast to other clients)
	std::cout << "pollserver: recv from fd " << sender_fd << ": ";
	std::cout.write(buf, 100); //nbytes
	std::cout << std::endl;

}

/** Process all existing connections. */
void	ServerManager::processConnections() {
    for (size_t i = 0; i < _pfds.size(); ++i) {

		// Handle errors first - don't try to read
        if (_pfds[i].revents & POLLERR) {
            std::cerr << "Poll error on socket " << _pfds[i].fd << std::endl;
            close(_pfds[i].fd);
            delFromPfds(i);
            if (i > 0) --i;  // Reexamine this slot
            continue;
        }

        // Check if someone's ready to read or client disconnected (or both)
        if (_pfds[i].revents & (POLLIN | POLLHUP)) {
			if (isListener(_pfds[i].fd))
			{
                // If we're the listener, it's a new connection
                handleNewConnection(_pfds[i].fd);
            } else {
				std::cout << "it's a regular client" << std::endl;
                // Otherwise we're just a regular client
				// So far only one listener - 0
                handleClientData(i);
            }
        }
    }
}

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
			// Run through connections
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
