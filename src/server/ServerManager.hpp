#ifndef SERVER_MANAGER_HPP
# define SERVER_MANAGER_HPP

# include "Server.hpp"
# include <poll.h>
# include <vector>
# include <map>

class ServerManager {
    public:
        ServerManager();
        ~ServerManager();

        void setupServers(std::vector<Server> server_configs);
        void runServers();

    private:
        std::vector<Server> _servers;
};

#endif