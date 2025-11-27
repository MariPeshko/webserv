#include "server/Server.hpp"
#include "parser/Config.hpp"
#include <string>
#include <iostream>
#include <string.h>
#include <signal.h>
#include "server/ServerManager.hpp"

int	main(int ac, char **argv) {

	// Ignore the SIGPIPE signal globally for this process
    signal(SIGPIPE, SIG_IGN);
    
	Config			config;
    ServerManager	server_manager;

    try {
        if (ac <= 2) {
            // if no config file provided, use default config file
            std::string config_file = (ac == 1 ? "configs/default.conf" : argv[1]);

            // parse config file and save parsed data
            config.parse(config_file);
            // set up servers
            server_manager.setupServers(config.getServerConfigs());
            // Run the main server loop.
            server_manager.runServers();

        } else {
            std::cout << "Error: wrong arguments. Usage: ./webserv [config_file]" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
