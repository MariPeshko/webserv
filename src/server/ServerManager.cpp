#include "ServerManager.hpp"

ServerManager::ServerManager() {
}

ServerManager::~ServerManager() {
}

void ServerManager::setupServers(std::vector<Server> server_configs) {
    _servers = server_configs;
    for (std::vector<Server>::iterator it = _servers.begin(); it != _servers.end(); it++) {
        //socket setup
        //bind socket
		if (it->setupServer() == -1) {
			// You can add error handling logic here.
			// For example, stop all servers from running or just ignore this one.
			std::cerr << "Error setting up a server. Skipping it." << std::endl;
		}
    }
}

void ServerManager::runServers() {
    for (std::vector<Server>::iterator it = _servers.begin(); it != _servers.end(); it++) {
        // poll(_servers, 0, 0);
        //accept connections
        //read requests
        //send responses
        
        //if cgi
        //if sendResponse
        
        //close connections
    }
}