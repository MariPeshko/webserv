#include "ServerManager.hpp"

ServerManager::ServerManager() {
}

ServerManager::~ServerManager() {
}

void ServerManager::setupServers(std::vector<ServerConfig> server_configs) {
    _servers = server_configs;
    for (std::vector<ServerConfig>::iterator it = _servers.begin(); it != _servers.end(); it++) {
        //socket setup
        //bind socket
        it->setupServer();
    }
}

void ServerManager::runServers() {
    for (std::vector<ServerConfig>::iterator it = _servers.begin(); it != _servers.end(); it++) {
        // poll(_servers, 0, 0);
        //accept connections
        //read requests
        //send responses
        
        //if cgi
        //if sendResponse
        
        //close connections
    }
}