#include <iostream>
#include <cstdlib>		// std::exit	
#include <sys/socket.h>	// socket, bind, listen, accept, send
#include <netinet/in.h>	// defines the sockaddr_in structure, htons
#include <unistd.h>		// read, close
#include <cstring>		// for strerror
#include <cerrno>		// for errno

#include <sys/epoll.h>
#include <vector>

void handle_new_connection(int server_fd, int epoll_fd, std::vector<int>& client_fds) {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd == -1) {
        std::cerr << "Accept failed: " << strerror(errno) << std::endl;
        return;
    }
    
    std::cout << "New client connected: " << client_fd << std::endl;
    
    // Add client socket to epoll
    struct epoll_event event;
    event.events = EPOLLIN;  // Monitor for input
    event.data.fd = client_fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
        std::cerr << "Failed to add client to epoll: " << strerror(errno) << std::endl;
        close(client_fd);
        return;
    }
    
    client_fds.push_back(client_fd);
}

void remove_client(int client_fd, int epoll_fd, std::vector<int>& client_fds) {
    // Remove from epoll
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    
    // Remove from client list
    for (std::vector<int>::iterator it = client_fds.begin(); it != client_fds.end(); ++it) {
        if (*it == client_fd) {
            client_fds.erase(it);
            break;
        }
    }
    
    close(client_fd);
    std::cout << "Client " << client_fd << " removed. Active clients: " << client_fds.size() << std::endl;
}

int main() {
    // ... existing socket setup code ...
    
    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cerr << "Failed to create epoll: " << strerror(errno) << std::endl;
        return 1;
    }
    
    // Add server socket to epoll
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);
    
    std::cout << "Server is listening with epoll..." << std::endl;
    
    struct epoll_event events[10];
    std::vector<int> client_fds;
    
    while (true) {
        // REQUIRED: Use poll/epoll before any socket I/O
        int nfds = epoll_wait(epoll_fd, events, 10, -1);
        if (nfds == -1) {
            std::cerr << "epoll_wait failed: " << strerror(errno) << std::endl;
            break;
        }
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                // New connection ready
                handle_new_connection(server_fd, epoll_fd, client_fds);
            } else {
                // Client data ready - NOW it's safe to read!
                handle_client_data(events[i].data.fd, epoll_fd, client_fds);
            }
        }
    }
    
    close(epoll_fd);
    close(server_fd);
    return 0;
}

void handle_client_data(int client_fd, int epoll_fd, std::vector<int>& client_fds) {
    char buffer[1024] = {0};
    // Safe to read now - epoll told us data is ready!
    int valread = read(client_fd, buffer, sizeof(buffer) - 1);
    
    if (valread > 0) {
        buffer[valread] = '\0';
        std::cout << "Received: " << buffer << std::endl;
        
        // For send(), you should check if socket is ready for writing
        // But for small responses, it usually works immediately
        ssize_t result = send(client_fd, buffer, valread, MSG_NOSIGNAL);
        if (result == -1) {
            if (errno == EPIPE || errno == ECONNRESET) {
                std::cout << "Client disconnected during send" << std::endl;
                remove_client(client_fd, epoll_fd, client_fds);
            }
        }
    } else if (valread == 0) {
        std::cout << "Client disconnected" << std::endl;
        remove_client(client_fd, epoll_fd, client_fds);
    } else {
        std::cerr << "Read error: " << strerror(errno) << std::endl;
        remove_client(client_fd, epoll_fd, client_fds);
    }
}