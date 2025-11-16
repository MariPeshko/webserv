#ifndef LOCATION_HPP
# define LOCATION_HPP

# include <iostream>
# include <string>
# include <vector>
# include <map>
# include <sys/socket.h>
# include <netinet/in.h>

class Location {
    public:
        Location();
        ~Location();

    private:
        std::string _path;
        std::string _root;
        std::string _index;
        bool _autoindex;
        std::vector<std::string> _methods;
        
};
#endif