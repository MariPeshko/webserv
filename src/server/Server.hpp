#ifndef SERVER_HPP
# define SERVER_HPP

# include <iostream>
# include <string>
# include <vector>
# include <map>

class Location {
	public:
		Location() : _autoindex(false) {};
		~Location() {};
		// We will add getters and other methods later

	private:
		std::string	_path;
		std::string _root;
		std::vector<std::string>	_allowed_nethods;
		std::string	_index;
		bool	_autoindex;

};

class Server {
    public:
        Server();
        ~Server();

        int setupServer();

    private:
        int _port;
        std::string _host;
        std::vector<std::string> _server_names;
        std::string _root;
        std::string _index;
        std::map<int, std::string> _error_pages;
        std::vector<Location> _locations;
};
#endif