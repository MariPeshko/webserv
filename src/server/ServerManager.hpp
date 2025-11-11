#ifndef SERVER_MANAGER_HPP
# define SERVER_MANAGER_HPP

# include "Server.hpp"
# include <map>
# include "ServerConfig.hpp"
# include <poll.h>

class ServerManager {
    public:
        ServerManager();
        ~ServerManager();

        void setupServers(std::vector<ServerConfig> server_configs);
        void runServers();

    private:
        std::vector<ServerConfig> _servers;
};
#endif