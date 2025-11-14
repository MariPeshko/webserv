#include "Config.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

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
    
    // Tokenize the config file
    std::vector<std::string> tokens = tokenize(config_file);
    
    // For now, let's just print the tokens to verify it works
    std::cout << "Tokens found:" << std::endl;
    for (size_t i = 0; i < tokens.size(); i++) {
        std::cout << "[" << i << "] " << tokens[i] << std::endl;
    }
}

