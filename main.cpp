#include <iostream>
#include <cstdlib>		// std::exit	
#include <sys/socket.h>	// socket, bind, listen, accept, send
#include <netinet/in.h>	// defines the sockaddr_in structure, htons
#include <unistd.h>		// read, close
#include <cstring>		// for strerror
#include <cerrno>		// for errno

/* it's simple TCP-server */

int	main() {

	int	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		std::cerr << "Failed to create socket " << strerror(errno) << std::endl;
		return 1;
	}
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
    server_addr.sin_family = AF_INET; // for IPv4 
    server_addr.sin_addr.s_addr = INADDR_ANY; // IP, i.e. inet_addr("127.0.0.1");
    server_addr.sin_port = htons(8080); // port number

    // Bind the socket to the specified IP and port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind socket " << strerror(errno) << std::endl;
        return 1;
    }
    std::cout << "Socket bound successfully" << std::endl;

	/**
	 * Listen for incoming connections
	 * 'backlog' is the maximum number of pending connections
	 * if the backlog is exceeded, new connections will be refused
	 * here we set it to 3, meaning up to 3 connections can be queued
	 */
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Failed to listen on socket " << strerror(errno) << std::endl;
        return 1;
    }
	
	std::cout << "Server is listening..." << std::endl;

	// Accept a client connection
	sockaddr_in	client_addr;
	socklen_t	client_addr_len = sizeof(client_addr);
	int			client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
	
	if (client_fd < 0) {
		std::cerr << "Failed to accept client connection " << strerror(errno) << std::endl;
		return 1;
	}

	std::cout << "Client connected" << std::endl;

	// Read data from the client and echo it back in a loop
	while (true) {
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
			break;
		} else {
			std::cerr << "Failed to read from client: " << strerror(errno) << std::endl;
			break;
		}
	}
	// Close client socket
	close(client_fd);
	
 	// Cleanup
    close(server_fd);

    return 0;
}
