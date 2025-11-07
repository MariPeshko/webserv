#include "Config.hpp"

// for now set up mock data
Config::Config() {
    _config_file = "configs/default.conf";
    _config_lines = std::vector<std::string>();
    _ready = std::vector<int>();
    _fd_set = fd_set();
    _fd_size = 0;
    _max_fd = 0;
}

Config::~Config() {
}

void Config::parse(const std::string& config_file) {
    // parse config file and save parsed data
    std::cout << "Parsing config file: " << config_file << std::endl;
}