#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    // Create a socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }



    std::cout << "Socket created successfully" << std::endl;

    // Prepare the sockaddr_in structure
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    // Bind the socket to the specified IP and port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind socket" << std::endl;
        return 1;
    }

    std::cout << "Socket bound successfully" << std::endl;

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        return 1;
    }

    std::cout << "Server is listening..." << std::endl;

    // Accept a client connection
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_fd < 0) {
        std::cerr << "Failed to accept client connection" << std::endl;
        return 1;
    }

    std::cout << "Client connected" << std::endl;

    // Read data from the client and echo it back
    char buffer[1024] = {0};
    int valread = read(client_fd, buffer, 1024);
    if (valread < 0) {
        std::cerr << "Failed to read from client" << std::endl;
    } else {
        send(client_fd, buffer, valread, 0);
        std::cout << "Echoed back: " << buffer << std::endl;
    }

    // Close the client socket
    close(client_fd);
    // Close the server socket
    close(server_fd);

    return 0;
}
