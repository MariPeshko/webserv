#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <iostream>
# include <string>
# include <vector>
# include <map>
# include <sys/socket.h>
# include <netinet/in.h>

class Client {
    public:
        Client();
        ~Client();

    private:
        int _fd;
        struct sockaddr_in _client_address;
};
#endif