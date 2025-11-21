#ifndef SERVER_MANAGER_HPP
# define SERVER_MANAGER_HPP

#include "Server.hpp"
#include <unistd.h>	// close()
#include <poll.h>
#include <vector>
#include <map>
#include <cstring>	// strerror()
#include <cerrno>
#include <sstream>	// for std::ostringstream
#include "Client.hpp"
#include <iostream>


class	ServerManager {
	public:
		ServerManager();
		~ServerManager();
		
		void	setupServers(std::vector<Server> & server_configs);
		void	runServers();
		
	private:
		// Vector of poll file descriptors
		std::vector<pollfd>		_pfds;
		// Map of Server's pointers (listener fd)
		std::map<int, Server*>	_map_servers;
		// 
		std::map<int, Client>	_clients;
		
		void	addToPfds(std::vector<pollfd>& pfds, int newfd);
		void	delFromPfds(size_t index);
		void	processConnections();
		void	handleErrorRevent(size_t i);
		void	handleNewConnection(int listener);
		void	handleClientData(size_t i);
		void	handleClientHungup(Client& client, size_t i);
		void	handleClientError(Client& client, size_t i);
		bool	isListener(int fd);
		void	sendResponse(Client& client);
};

#endif