#ifndef CONFIG_HPP
# define CONFIG_HPP

# include "../server/Server.hpp"
# include <iostream>
# include <string>
# include <vector>
# include <fstream>
# include <map>

class Config {
    public:
        Config();
        ~Config();
        void parse(const std::string& config_file);
        std::vector<ServerConfig> getServerConfigs();

    private:
        std::string _config_file;
        std::vector<std::string> _config_lines;
        std::map<long, Server>		_servers;
	    std::map<long, Server *>	_sockets;
	    std::vector<int>			_ready;
	    fd_set						_fd_set;
	    unsigned int				_fd_size;
	    long						_max_fd;
};
#endif