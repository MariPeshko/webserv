#include "server/Server.hpp"
#include "server/Config.hpp"
#include <string>
#include <iostream>
#include <string.h>
#include <signal.h>
#include "server/ServerManager.hpp"

static volatile bool	g_shutdown = false;
static ServerManager*	g_server_manager = NULL;

void	signalHandler(int signal_num) {
	switch (signal_num) {
		case SIGINT:
		case SIGTERM:
			g_shutdown = true;
			if (g_server_manager) {
				g_server_manager->requestShutdown();
			}
			break;
		case SIGQUIT:
			g_shutdown = true;
			break;
		default:
			break;
	}
}

int		main(int ac, char **argv) {

	// Ignore the SIGPIPE signal globally for this process
	signal(SIGPIPE, SIG_IGN);

	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGQUIT, signalHandler);
	
	Config			config;
	ServerManager	server_manager;

	// Set global pointer for signal handler access
	g_server_manager = &server_manager;

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
			g_server_manager = NULL;
			return 1;
		}
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		g_server_manager = NULL;
		return 1;
	}
    g_server_manager = NULL; // Clean up global pointer
	return 0;
}
