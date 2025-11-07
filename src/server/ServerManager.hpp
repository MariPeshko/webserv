#ifndef SERVER_MANAGER_HPP
# define SERVER_MANAGER_HPP

# include "Server.hpp"
# include <map>
# include "ServerConfig.hpp"

class ServerManager {
    public:
        ServerManager();
        ~ServerManager();

        void setupServers(std::vector<ServerConfig> server_configs);
        void removeServer(int server_fd);
        void start();
        void stop();

    private:
        std::map<int, Server > _servers;
};
#endif