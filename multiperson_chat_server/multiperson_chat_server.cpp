/*
** multiperson_chat_server_cpp.cpp -- a multiperson chat server in C++98
** Converted from Beej's Guide to Network Programming C example
*/

#include <iostream>
#include <sstream>		// for std::ostringstream
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>		// struct addrinfo, freeaddrinfo
#include <arpa/inet.h>	// htons, htonl, ntohs, ntohl
#include <poll.h>		// poll, strcut pollfd
#include <signal.h>

const char* PORT = "9034";   // Port we're listening on

// Global variables for signal handling
volatile bool			g_running = true;
std::vector<pollfd>*	g_pfds = NULL;

/*
 * Signal handler for graceful shutdown
 */
void signal_handler(int sig) {
    std::cout << "\nReceived signal " << sig << ", shutting down gracefully..." << std::endl;
    g_running = false;
}

/** Return a listening socket. */
int	get_listener_socket() {
	int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
        return -1;
    }
    
    int yes = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        std::cerr << "Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
    }
	
	struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all IPv4 addresses
    server_addr.sin_port = htons(9034);        // Convert port to network byte order
	
	if (bind(listener, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        close(listener);
        return -1;
    }
    if (listen(listener, 10) == -1) {
        std::cerr << "Failed to listen: " << strerror(errno) << std::endl;
        return -1;
    }
    return listener;
}

/** Add a new file descriptor to the vector. */
void	add_to_pfds(std::vector<pollfd>& pfds, int newfd) {
	pollfd	pfd;
	pfd.fd = newfd;
	pfd.events = POLLIN; // Check ready-to-read
	pfd.revents = 0;
	
	pfds.push_back(pfd);
}

/** Remove a file descriptor at a given index from the vector. */
void	del_from_pfds(std::vector<pollfd>& pfds, size_t index) {
    if (index < pfds.size()) {
        pfds.erase(pfds.begin() + index);
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
void	handle_new_connection(int listener, std::vector<pollfd>& pfds) {
	
	struct sockaddr_in		remoteaddr;  // IPv4 only
	socklen_t				addrlen = sizeof(remoteaddr);;
	int						newfd;		// Newly accept()ed socket descriptor
	
	// accept() fills remoteaddr with actual client address
	newfd = accept(listener, reinterpret_cast<struct sockaddr*>(&remoteaddr), &addrlen);
	//                       ^^^^^^^^^^^^ Cast to base type for API

	if (newfd == -1) {
        std::cerr << "Accept failed: " << strerror(errno) << std::endl;
    } else {
        add_to_pfds(pfds, newfd);

		uint32_t ip = ntohl(remoteaddr.sin_addr.s_addr);
		std::string ip_str = ipv4_to_string(ip);
		
		std::cout << "pollserver: new connection from " << ip_str 
				<< " on socket " << newfd << std::endl;
    }
}

/** Handle regular client data or client hangups. */
void	handle_client_data(int listener, std::vector<pollfd>& pfds, size_t& i) {
	
	char	buf[256];    // Buffer for client data

	// In case of POLLHUP - Client hung up (disconnected) recv() returns 0 (EOF)
	int		nbytes = recv(pfds[i].fd, buf, sizeof(buf), 0);
	int		sender_fd = pfds[i].fd;
	
	if (nbytes <= 0) { // Got error or connection closed by client
        if (nbytes == 0) {
            // Connection closed
            std::cout << "pollserver: socket " << sender_fd << " hung up" << std::endl;
        } else {
			std::cout << "pollserver: socket " << sender_fd << " error" << std::endl;  // ✅ No errno
            //std::cerr << "Recv error: " << strerror(errno) << std::endl;
        }

        close(pfds[i].fd);
        del_from_pfds(pfds, i);
        if (i > 0) --i; // reexamine the slot we just deleted
		return;
    }
	
	// We got some good data from a client (broadcast to other clients)
	std::cout << "pollserver: recv from fd " << sender_fd << ": ";
	std::cout.write(buf, nbytes);
	
	// Send to everyone!
	for (size_t j = 0; j < pfds.size(); ++j) {
		int dest_fd = pfds[j].fd;

		// Except the listener and ourselves
		if (dest_fd != listener && dest_fd != sender_fd) {
			if (send(dest_fd, buf, nbytes, MSG_NOSIGNAL) == -1) {
				if (errno == EPIPE || errno == ECONNRESET) { // ❌ FORBIDDEN!
					std::cout << "Client " << dest_fd << " disconnected during send" << std::endl;
				} else {
					std::cerr << "Send error: " << strerror(errno) << std::endl;
				}
			}
		}
	}
}

/** Process all existing connections. */
void	process_connections(int listener, std::vector<pollfd>& pfds) {
    for (size_t i = 0; i < pfds.size(); ++i) {

		// Handle errors first - don't try to read
        if (pfds[i].revents & POLLERR) {
            std::cerr << "Poll error on socket " << pfds[i].fd << std::endl;
            close(pfds[i].fd);
            del_from_pfds(pfds, i);
            if (i > 0) --i;  // Reexamine this slot
            continue;
        }

        // Check if someone's ready to read or client disconnected (or both)
        if (pfds[i].revents & (POLLIN | POLLHUP)) {
            // We got one!!

            if (pfds[i].fd == listener) {
                // If we're the listener, it's a new connection
                handle_new_connection(listener, pfds);
            } else {
                // Otherwise we're just a regular client
                handle_client_data(listener, pfds, i);
            }
        }
    }
}

/*
 * Main: create a listener and connection vector, loop forever
 * processing connections. */
int	main() {
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGPIPE, SIG_IGN);  // Ignore SIGPIPE
	
	std::cout << "Server PID: " << getpid() << std::endl;
	
	int					listener;	// Listening socket descriptor
	std::vector<pollfd>	pfds;		// Vector of poll file descriptors
	g_pfds = &pfds;					// For signal handler access
	
	// Set up and get a listening socket
	listener = get_listener_socket();
	if (listener == -1) {
        std::cerr << "Error getting listening socket" << std::endl;
        return 1;
    }

    // Add the listener to set
    add_to_pfds(pfds, listener);

    std::cout << "pollserver: waiting for connections..." << std::endl;

    // Main loop
	while (g_running) {
		// poll() returns the number of elements in the pfds
		// array for which events have occurred
		int	poll_count = poll(&pfds[0], pfds.size(), 1000);  // 1 second timeout
		
		if (poll_count == -1) {
			if (!g_running) break;  // Signal received
			std::cerr << "Poll error: " << strerror(errno) << std::endl;
			break;
		}
		
		if (poll_count > 0) {
			// Run through connections looking for data to read
			process_connections(listener, pfds);
		}
	}
	
	// Cleanup
    std::cout << "Closing all connections..." << std::endl;
    for (size_t i = 0; i < pfds.size(); ++i) {
        close(pfds[i].fd);
    }
	std::cout << "Server shut down cleanly" << std::endl;
	return 0;
}
