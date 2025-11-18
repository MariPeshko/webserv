#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "../response/Response.hpp"
# include "../request/Request.hpp"
# include <iostream>
# include <string>
# include <vector>
# include <map>
# include <sys/socket.h>
# include <netinet/in.h>

class Client {
    public:
        Client();
        Client(const Client &other);
        ~Client();

        void setFd(int fd);

   

    private:
        int _fd;
        struct sockaddr_in _client_address;
        Response    response;
        Request     request; 
};
#endif