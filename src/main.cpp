#include "server/Server.hpp"
#include "server/Config.hpp"
#include <string>
#include <iostream>
#include <string.h>
#include <signal.h>
#include "server/ServerManager.hpp"
#include "logger/Logger.hpp"

int	main(int ac, char **argv) {

	// Ignore the SIGPIPE signal globally for this process
	signal(SIGPIPE, SIG_IGN);

    Logger::init("webserv.log");
    Logger::log(LOG_INFO, "Webserv started");
	
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
			Logger::log(LOG_ERROR, "Wrong arguments. Usage: ./webserv [config_file]");
            return 1;
        }
    } catch (const std::exception& e) {
		Logger::log(LOG_ERROR, "Exception: " + std::string(e.what()));
        return 1;
    }
    return 0;
}
