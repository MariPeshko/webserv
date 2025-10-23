#ifndef SERVER_HPP
# define SERVER_HPP

# include <iostream>
# include <string>

class Server {
    public:
        Server();
        Server(int port, std::string host, std::string name, std::string root, std::string index, std::string error_page, std::string cgi, std::string location);
        ~Server();

    private:
        int _port;
        std::string _host;
        std::string _name;
        std::string _root;
        std::string _index;
        std::string _error_page;
        std::string _cgi;
        std::string _location;
};
#endif