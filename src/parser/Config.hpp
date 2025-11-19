#ifndef CONFIG_HPP
# define CONFIG_HPP

# include "../server/Server.hpp"
# include <string>
# include <vector>
# include <fstream>
# include <sstream>
# include <stdexcept>
# include <cctype>
# include <cstdlib>
# include <algorithm>

class Config {
    public:
        Config();
        ~Config();
        void parse(const std::string& config_file);
        const std::vector<Server>& getServerConfigs() const;

    private:
        std::vector<Server> _servers;

        // Token-based parsing methods
        std::vector<std::string> tokenize(const std::string& content);
		void parseServer(Server& server, std::vector<std::string>& tokens);
		void parseLocation(Location& location, std::vector<std::string>& tokens);
};

#endif
