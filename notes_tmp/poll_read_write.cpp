#include <iostream>
#include <cstdlib>		// std::exit	
#include <sys/socket.h>	// socket, bind, listen, accept, send
#include <netinet/in.h>	// defines the sockaddr_in structure, htons
#include <unistd.h>		// read, close
#include <cstring>		// for strerror
#include <cerrno>		// for errno

// Reading configuration files
int config_fd = open("webserv.conf", O_RDONLY);
read(config_fd, buffer, size);  // OK! No poll() needed

// Reading static HTML files  
int html_fd = open("index.html", O_RDONLY);
read(html_fd, buffer, size);   // OK! No poll() needed

// Writing log files
int log_fd = open("access.log", O_WRONLY | O_APPEND);
write(log_fd, log_data, size);  // OK! No poll() needed

// ❌ FORBIDDEN without poll():

// Your current socket code - MUST change this!
int client_fd = accept(server_fd, ...);
read(client_fd, buffer, size);  // FORBIDDEN! Need poll() first

send(client_fd, response, size); // FORBIDDEN! Need poll() first

// Your Current Code Needs Changes:

// This is now FORBIDDEN:
while (true) {
    int valread = read(client_fd, buffer, sizeof(buffer) - 1);  // No poll() before read!
    // ...
    send(client_fd, buffer, valread, MSG_NOSIGNAL);  // No poll() before send!
}

