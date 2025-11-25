#ifndef SERVER_MANAGER_HPP
# define SERVER_MANAGER_HPP

#include "Server.hpp"
#include "../httpContext/Connection.hpp"
#include "HttpContext.hpp"
#include <unistd.h>	// close()
#include <poll.h>
#include <vector>
#include <map>
#include <cstring>	// strerror()
#include <cerrno>
#include <sstream>	// for std::ostringstream
#include <iostream>


class	ServerManager {
	public:
		ServerManager();
		~ServerManager();
		
		void	setupServers(std::vector<Server> & server_configs);
		void	runServers();
		
	private:
		std::vector<pollfd>		_pfds;
		std::map<int, Server*>	_map_servers;

		// REFACTORED: replace _clients with connections + contexts
		std::map<int, Connection>	_connections;
		std::map<int, HttpContext>	_contexts;
		
		void	addToPfds(std::vector<pollfd>& pfds, int newfd);
		void	delFromPfds(size_t index);
		void	processConnections();
		void	handleNewConnection(int listener);
		void	handleClientData(size_t i);
		void	handleErrorRevent(int fd, size_t i);
		void	handleClientError(int fd, size_t i);
		void	handleClientHungup(int fd, size_t i);
		bool	isListener(int fd);
		void	sendResponse(int fd, HttpContext& ctx);
};

#endif