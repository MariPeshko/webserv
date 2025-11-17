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

class	ServerManager {
	public:
		ServerManager();
		~ServerManager();
		
		void	setupServers(std::vector<Server> server_configs);
		void	runServers();
		
	private:
		std::vector<Server>		_servers;
		std::map<int, Server*>	_map_servers; // Map listener fd to Server
		std::vector<pollfd>		_pfds;	// Vector of poll file descriptors
		void				add_to_pfds(std::vector<pollfd>& pfds, int newfd);
		void				delFromPfds(size_t index);
		void				processConnections();
		void				handleNewConnection(int listener);
		void				handleClientData(size_t& i);
		bool				isListener(int fd);
};

#endif