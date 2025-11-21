#include "Config.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept> // runtime_error

// Return parsed servers
std::vector<Server> & Config::getServerConfigs() {
    return _servers;
}

std::vector<std::string> Config::tokenize(const std::string& config_file)
{
	std::vector<std::string> tokens;
	std::ifstream file(config_file.c_str()); 

	if (!file.is_open()) {
		throw std::runtime_error("Error: Could not open config file: " + config_file);
	}

	std::string line;
	while (std::getline(file, line)) {
		// Remove comments
		size_t comment_pos = line.find('#');
		if (comment_pos != std::string::npos) {
			line = line.substr(0, comment_pos);
		}
		
		// Trim whitespace from both ends
		while (!line.empty() && std::isspace(line[0])) {
			line.erase(0, 1);
		}
		while (!line.empty() && std::isspace(line[line.length() - 1])) {
			line.erase(line.length() - 1, 1);
		}
		
		// Skip empty lines
		if (line.empty()) {
			continue;
		}
		
		// Split the line into tokens
		std::string current_token;
		for (size_t i = 0; i < line.length(); i++) {
			char c = line[i];
			
			// If special character
			if (c == '{' || c == '}' || c == ';' || c == '[' || c == ']' || c == ',') {
				if (!current_token.empty()) {
					tokens.push_back(current_token);
					current_token.clear();
				}
				// Add special character as its own token
				tokens.push_back(std::string(1, c));
			}
			// If whitespace
			else if (std::isspace(c)) {
				// Save current token if it exists
				if (!current_token.empty()) {
					tokens.push_back(current_token);
					current_token.clear();
				}
				// Skip the whitespace
			}
			// Regular character - add to current token
			else {
				current_token += c;
			}
		}
		
		// last token if line doesn't end with special char
		if (!current_token.empty()) {
			tokens.push_back(current_token);
		}
	}
	
	file.close();
	return tokens;
}


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
    _config_file = config_file;
    
    // Tokenize  config file
    std::vector<std::string> tokens = tokenize(config_file);
    
    // Create a default server
    Server server;
    
    // Parse tokens
    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i] == "location") {
            // Parse location block
            parseLocationBlock(server, tokens, i);
        } else {
            // Parse server-level directive
            parseServerDirective(server, tokens, i);
        }
    }
	
	// Add  server to list
	_servers.push_back(server);
	
	// Hard-coded server #2 (Maryna)
	Server	server2(server);
	server2.setPort(8081);
	_servers.push_back(server2);
	
	std::cout << "Parsed " << _servers.size() << " server(s)" << std::endl;
	std::cout << "Server has " << _servers[0].getLocationCount() << " location(s)" << std::endl;
	
	// Print parsed configuration for verification
	std::cout << "\n=== Parsed Configuration ===" << std::endl;
	_servers[0].print();
	std::cout << "============================\n" << std::endl;
	_servers[0].print();
	std::cout << "============================\n" << std::endl;
}

// Helper: Parse array like [GET, POST]
std::vector<std::string> Config::parseArray(const std::vector<std::string>& tokens, size_t& i) {
    std::vector<std::string> result;
    
    if (i >= tokens.size() || tokens[i] != "[") {
        throw std::runtime_error("Expected '[' for array");
    }
    i++; // Skip '['
    
    while (i < tokens.size() && tokens[i] != "]") {
        if (tokens[i] != ",") {
            result.push_back(tokens[i]);
        }
        i++;
    }
    
    if (i >= tokens.size() || tokens[i] != "]") {
        throw std::runtime_error("Expected ']' to close array");
    }
    i++; // Skip ']'
    
    return result;
}

// Helper: Parse server-level directives
void Config::parseServerDirective(Server& server, const std::vector<std::string>& tokens, size_t& i) {
    if (i >= tokens.size()) return;
    
    std::string directive = tokens[i];
    
    if (directive == "server_name") {
        i++; // Skip directive name
        while (i < tokens.size() && tokens[i] != ";") {
            if (tokens[i] != ",") {
                server.addServerName(tokens[i]);
            }
            i++;
        }
        if (i < tokens.size() && tokens[i] == ";") i++; // Skip ';'
    }
    else if (directive == "error_page") {
        i++; // Skip directive name
        if (i >= tokens.size()) {
            throw std::runtime_error("error_page: missing error code");
        }
        int error_code = atoi(tokens[i].c_str());
        i++;
        if (i >= tokens.size()) {
            throw std::runtime_error("error_page: missing error page path");
        }
        std::string error_path = tokens[i];
        server.addErrorPage(error_code, error_path);
        i++;
        if (i < tokens.size() && tokens[i] == ";") i++; // Skip ';'
    }
    else if (directive == "client_max_body_size") {
        i++; // Skip directive name
        if (i >= tokens.size()) {
            throw std::runtime_error("client_max_body_size: missing value");
        }
        server.setClientMaxBodySize(tokens[i]);
        i++;
        if (i < tokens.size() && tokens[i] == ";") i++; // Skip ';'
    }
}

// Helper: Parse a location block
void Config::parseLocationBlock(Server& server, const std::vector<std::string>& tokens, size_t& i) {
    Location location;
    
    i++; // Skip "location"
    if (i >= tokens.size()) {
        throw std::runtime_error("location: missing path");
    }
    location.setPath(tokens[i]);
    i++;
    
    if (i >= tokens.size() || tokens[i] != "{") {
        throw std::runtime_error("location: expected '{'");
    }
    i++; // Skip '{'
    
    // Parse location directives until '}'
    while (i < tokens.size() && tokens[i] != "}") {
        parseLocationDirective(location, tokens, i);
    }
    
    if (i >= tokens.size() || tokens[i] != "}") {
        throw std::runtime_error("location: expected '}'");
    }
    i++; // Skip '}'
    
    server.addLocation(location);
}

// Helper: Parse location-level directives
void Config::parseLocationDirective(Location& location, const std::vector<std::string>& tokens, size_t& i) {
    if (i >= tokens.size()) return;
    
    std::string directive = tokens[i];
    
    if (directive == "methods") {
        i++; // Skip directive name
        std::vector<std::string> methods = parseArray(tokens, i);
        for (size_t j = 0; j < methods.size(); j++) {
            location.addAllowedMethod(methods[j]);
        }
        if (i < tokens.size() && tokens[i] == ";") i++; // Skip ';'
    }
    else if (directive == "root") {
        i++; // Skip directive name
        if (i >= tokens.size()) {
            throw std::runtime_error("root: missing path");
        }
        location.setRoot(tokens[i]);
        i++;
        if (i < tokens.size() && tokens[i] == ";") i++; // Skip ';'
    }
    else if (directive == "index") {
        i++; // Skip directive name
        if (i >= tokens.size()) {
            throw std::runtime_error("index: missing value");
        }
        // Take first index value
        location.setIndex(tokens[i]);
        i++;
        // Skip remaining index values until ';'
        while (i < tokens.size() && tokens[i] != ";") {
            i++;
        }
        if (i < tokens.size() && tokens[i] == ";") i++; // Skip ';'
    }
    else if (directive == "autoindex") {
        i++; // Skip directive name
        if (i >= tokens.size()) {
            throw std::runtime_error("autoindex: missing value");
        }
        bool autoindex = (tokens[i] == "on");
        location.setAutoindex(autoindex);
        i++;
        if (i < tokens.size() && tokens[i] == ";") i++; // Skip ';'
    }
    else if (directive == "return") {
        i++; // Skip directive name
        if (i >= tokens.size()) {
            throw std::runtime_error("return: missing code");
        }
        int code = atoi(tokens[i].c_str());
        i++;
        if (i >= tokens.size()) {
            throw std::runtime_error("return: missing URL");
        }
        location.setReturn(code, tokens[i]);
        i++;
        if (i < tokens.size() && tokens[i] == ";") i++; // Skip ';'
    }
    else if (directive == "cgi") {
        i++; // Skip directive name
        if (i >= tokens.size()) {
            throw std::runtime_error("cgi: missing extension");
        }
        std::string ext = tokens[i];
        i++;
        if (i >= tokens.size()) {
            throw std::runtime_error("cgi: missing executable path");
        }
        location.addCgi(ext, tokens[i]);
        i++;
        if (i < tokens.size() && tokens[i] == ";") i++; // Skip ';'
    }
    // Ignore unknown directives
}

