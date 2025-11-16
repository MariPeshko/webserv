#include "server/Server.hpp"
#include "parser/Config.hpp"
#include <string>
#include <iostream>
#include <string.h>

int	main(int ac, char **argv) {
    Config config;
    if (ac == 1 || ac == 2) {
        // if no config file provided, use default config file
        std::string config_file = (ac == 1 ? "configs/default.conf" : argv[1]);

        // parse config file and save parsed data
        config.parse(config_file);
        // set up server

    } else {
        std::cout << "Error: wrong arguments" << std::endl;
        return 1;
    }
    return 0;
}
