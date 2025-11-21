#ifndef CONFIG_HPP
# define CONFIG_HPP

# include "../server/Server.hpp"
# include <iostream>
# include <string>
# include <vector>
# include <fstream>
# include <map>
# include <sys/select.h> 

class Config {
	public:
		Config();
		~Config();
		void parse(const std::string& config_file);
		std::vector<Server> &	getServerConfigs();

	private:
		std::string					_config_file;
		std::vector<std::string>	_config_lines;
		std::vector<Server>			_servers; // parsed servers
		std::map<long, Server *>	_sockets;
		std::vector<int>			_ready;
		fd_set						_fd_set;
		unsigned int				_fd_size;
		long						_max_fd;

		std::vector<std::string> tokenize(const std::string& config_file);
		
		// Parser helper functions
		void parseServerDirective(Server& server, const std::vector<std::string>& tokens, size_t& i);
		void parseLocationBlock(Server& server, const std::vector<std::string>& tokens, size_t& i);
		void parseLocationDirective(Location& location, const std::vector<std::string>& tokens, size_t& i);
		std::vector<std::string> parseArray(const std::vector<std::string>& tokens, size_t& i);
};

#endif
