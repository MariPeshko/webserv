#include "server/Server.hpp"
#include "server/Config.hpp"
#include <string>
#include <iostream>
#include <string.h>
#include <signal.h>
#include "server/ServerManager.hpp"
#include "logger/Logger.hpp"

static ServerManager*	g_server_manager = NULL;

/**
 * Note: Same scenario for all three signals. It' a fall-through 
 * in C/C++ switch statements. When there is no break; between cases,
 * they all execute the same code.
 */
void	signalHandler(int signal_num) {
	switch (signal_num) {
		case SIGINT:
		case SIGTERM:
		case SIGQUIT:
			if (g_server_manager) {
				g_server_manager->requestShutdown();
			}
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

    Logger::init("webserv.log");
    Logger::log(LOG_INFO, "Webserv started");
	
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
			server_manager.setupServers(config.getServerConfigs());
			server_manager.runServers();
        } else {
			Logger::log(LOG_ERROR, "Wrong arguments. Usage: ./webserv [config_file]");
			g_server_manager = NULL;
            return 1;
        }
    } catch (const std::exception& e) {
		Logger::log(LOG_ERROR, "Exception: " + std::string(e.what()));
		g_server_manager = NULL;
        return 1;
    }
    g_server_manager = NULL;
    return 0;
}
