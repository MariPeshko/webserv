#ifndef SERVER_MANAGER_HPP
# define SERVER_MANAGER_HPP

#include "Server.hpp"
#include "../httpContext/Connection.hpp"
#include "../httpContext/HttpContext.hpp"
#include <unistd.h>	// close()
#include <poll.h>
#include <vector>
#include <map>
#include <cstring>	// strerror()
#include <cerrno>
#include <sstream>	// for std::ostringstream
#include <iostream>
#include <fcntl.h>

#define GREEN "\033[32m"
#define RESET "\033[0m"

class	ServerManager {
	public:
		ServerManager();
		~ServerManager();

		void	setupServers(std::vector<Server> & server_configs);
		void	runServers();
		void	removeClient(int fd, size_t i);
		bool	isShutdownRequested() const;
		void	requestShutdown();
		
	private:
		std::vector<pollfd>			_pfds;
		std::map<int, Server*>		_map_servers;
		std::map<int, HttpContext>	_contexts;
		volatile bool				shutdown;
		
		void	addToPfds(std::vector<pollfd>& pfds, int newfd);
		void	delFromPfds(size_t index);
		void	processConnections();
		void	handleNewConnection(int listener);
		void	handleClientData(size_t i);
		void	handleClientWrite(size_t i);
		void	handleErrorRevent(int fd, size_t i);
		void	handleClientError(int fd, size_t i);
		void	handleClientHungup(int fd, size_t i);
		bool	isListener(int fd);
		void	checkTimeouts();
		void	cleanup();
};

#endif
