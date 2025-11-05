#include <iostream>
#include <cstdlib>		// std::exit	
#include <sys/socket.h>	// socket, bind, listen, accept, send
#include <netinet/in.h>	// defines the sockaddr_in structure, htons
#include <unistd.h>		// read, close
#include <cstring>		// for strerror
#include <cerrno>		// for errno
#include <signal.h>		// signal

/* it's simple TCP-server with basic signal handling by Maryna  */

// Global variable to store server socket
int				g_server_fd = -1;
int				g_current_client_fd = -1;
volatile bool	g_running = true;

// Signal handler for graceful shutdown
void	signal_handler(int sig) {
	std::cout << "\nReceived signal " << sig << ", shutting down gracefully..." << std::endl;
	g_running = false;  // Stop the server loop
	if (g_current_client_fd != -1) {
        close(g_current_client_fd);
        g_current_client_fd = -1;
    }
	if (g_server_fd != -1) {
        close(g_server_fd);
        g_server_fd = -1;
    }
	// std::exit(0);
	return ;
}

void sigpipe_handler(int sig) {
	(void)sig;
    // Just log it, don't terminate
    std::cout << "SIGPIPE received - client disconnected during write" << std::endl;
}

int	main() {
	// Set up signal handlers
	signal(SIGINT, signal_handler);   // Handle Ctrl+C
	signal(SIGTERM, signal_handler);  // Handle termination
	signal(SIGPIPE, sigpipe_handler);  // Ignore SIGPIPE - don't crash the server!

	std::cout << "Server PID: " << getpid() << std::endl;

	// Create a socket
    int	server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Failed to create socket " << strerror(errno) << std::endl;
        return 1;
    }

	g_server_fd = server_fd;  // Store globally for signal handler
	
	/**
     * Allow address reuse with setsockopt() SO_REUSEADDR
     * 
     * When you stop your server and restart it immediately, 
     * you might get "Address already in use" error.
     * This happens because the port is in TIME_WAIT state
     */
    int	opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set SO_REUSEADDR" << std::endl;
        close(server_fd);
        return 1;
    }

    std::cout << "Socket created successfully" << std::endl;

    // Prepare the sockaddr_in structure
	sockaddr_in	server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    // Bind the socket to the specified IP and port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind socket " << strerror(errno) << std::endl;
        return 1;
    }

    std::cout << "Socket bound successfully" << std::endl;

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Failed to listen on socket " << strerror(errno) << std::endl;
        return 1;
    }
	
	std::cout << "Server is listening..." << std::endl;

	while (g_running) {  // Main server loop
		// Accept a client connection
		sockaddr_in	client_addr;
		socklen_t	client_addr_len = sizeof(client_addr);
		int			client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
		
		if (!g_running) {
			if (g_server_fd != -1)
				close(g_server_fd); // Close the server socket
			close(client_fd);
			return 0;
		}
		if (client_fd < 0) {
			std::cerr << "Failed to accept client connection " << strerror(errno) << std::endl;
			return 1;
		}

		std::cout << "Client connected" << std::endl;
		g_current_client_fd = client_fd;  // Store globally

		// Read data from the client and echo it back in a loop
		while (g_running) {
			char	buffer[1024] = {0};
			int		valread = read(client_fd, buffer, sizeof(buffer) - 1);
			if (valread > 0) {
				buffer[valread] = '\0';
				// edge case: Client disconnects during a send operation
				ssize_t result = send(client_fd, buffer, valread, MSG_NOSIGNAL);
				if (result == -1) {
					if (errno == EPIPE || errno == ECONNRESET) {
						std::cout << "Client disconnected during send" << std::endl;
						break;
					} else {
						std::cerr << "Send error: " << strerror(errno) << std::endl;
					}
				} else {
					std::cout << "Echoed back: " << buffer << std::endl;
				}
			} else if (valread == 0) {
				std::cout << "Client disconnected" << std::endl;
				g_current_client_fd = -1;
				break;
			} else {
				if (!g_running) {
					std::cout << "Server shutting down..." << std::endl;
				} else {
					std::cerr << "Failed to read from client: " << strerror(errno) << std::endl;
				}
				break;
			}
		}

		// Close client socket
		close(client_fd);
	}
	
 	// Cleanup
    if (g_server_fd != -1) {
        close(g_server_fd);
        g_server_fd = -1;
    }

    return 0;
}
