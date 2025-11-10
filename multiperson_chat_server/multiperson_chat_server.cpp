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

/*
 * Convert socket to IP address string.
 * addr: struct sockaddr_in or struct sockaddr_in6
 */
const char*	inet_ntop2(void* addr, char* buf, size_t size) {

	struct sockaddr_storage*	sas = static_cast<struct sockaddr_storage*>(addr);
	struct sockaddr_in*			sa4;
	struct sockaddr_in6*		sa6;
	void*						src;

	switch (sas->ss_family) {
        case AF_INET:
            sa4 = static_cast<struct sockaddr_in*>(addr);
            src = &(sa4->sin_addr);
            break;
        case AF_INET6:
            sa6 = static_cast<struct sockaddr_in6*>(addr);
            src = &(sa6->sin6_addr);
            break;
        default:
            return NULL;
    }
	return inet_ntop(sas->ss_family, src, buf, size);
}

/** Return a listening socket. */
int	get_listener_socket() {
	int	listener;     // Listening socket descriptor
	int	yes = 1;      // For setsockopt() SO_REUSEADDR
	int	rv;
	
	struct addrinfo	hints, *ai, *p;

    // Get us a socket and bind it
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        std::cerr << "pollserver: " << gai_strerror(rv) << std::endl;
        return -1;
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // Lose the pesky "address already in use" error message
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
            std::cerr << "Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
        }

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // If we got here, it means we didn't get bound
    if (p == NULL) {
        freeaddrinfo(ai);
        return -1;
    }

    freeaddrinfo(ai); // All done with this

    // Listen
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
	
	struct sockaddr_storage	remoteaddr;	// Client address
	socklen_t				addrlen;
	int						newfd;		// Newly accept()ed socket descriptor
	
	addrlen = sizeof(remoteaddr);
	newfd = accept(listener, reinterpret_cast<struct sockaddr*>(&remoteaddr), &addrlen);
	
	if (newfd == -1) {
        std::cerr << "Accept failed: " << strerror(errno) << std::endl;
    } else {
        add_to_pfds(pfds, newfd);

		// Get client info for IPv4 only
        if (remoteaddr.ss_family == AF_INET) {
            struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(&remoteaddr);
            uint32_t ip = ntohl(addr_in->sin_addr.s_addr);  // ntohl is allowed!
            std::string ip_str = ipv4_to_string(ip);
            
            std::cout << "pollserver: new connection from " << ip_str 
                      << " on socket " << newfd << std::endl;
        } else {
            std::cout << "pollserver: new connection on socket " << newfd << std::endl;
        }
    }
}

/** Handle regular client data or client hangups. */
void	handle_client_data(int listener, std::vector<pollfd>& pfds, size_t& i) {
	
	char	buf[256];    // Buffer for client data
	
	int	nbytes = recv(pfds[i].fd, buf, sizeof(buf), 0);
	int	sender_fd = pfds[i].fd;
	
	if (nbytes <= 0) { // Got error or connection closed by client
        if (nbytes == 0) {
            // Connection closed
            std::cout << "pollserver: socket " << sender_fd << " hung up" << std::endl;
        } else {
            std::cerr << "Recv error: " << strerror(errno) << std::endl;
        }

        close(pfds[i].fd); // Bye!
        del_from_pfds(pfds, i);

        // reexamine the slot we just deleted
        if (i > 0) {
            --i;
        }

    } else { // We got some good data from a client
        std::cout << "pollserver: recv from fd " << sender_fd << ": ";
        std::cout.write(buf, nbytes);
        
        // Send to everyone!
        for (size_t j = 0; j < pfds.size(); ++j) {
            int dest_fd = pfds[j].fd;

            // Except the listener and ourselves
            if (dest_fd != listener && dest_fd != sender_fd) {
                if (send(dest_fd, buf, nbytes, MSG_NOSIGNAL) == -1) {
                    if (errno == EPIPE || errno == ECONNRESET) {
                        std::cout << "Client " << dest_fd << " disconnected during send" << std::endl;
                    } else {
                        std::cerr << "Send error: " << strerror(errno) << std::endl;
                    }
                }
            }
        }
    }
}

/** Process all existing connections. */
void	process_connections(int listener, std::vector<pollfd>& pfds) {
    for (size_t i = 0; i < pfds.size(); ++i) {
        // Check if someone's ready to read
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
