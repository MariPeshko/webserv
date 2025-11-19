#include "Config.hpp"
#include <iostream>
#include <algorithm>

Config::Config() {}
Config::~Config() {}

const std::vector<Server>& Config::getServerConfigs() const {
    return _servers;
}

// Tokenizer: Converts the raw configuration string into a vector of tokens.
std::vector<std::string> Config::tokenize(const std::string& content) {
    std::vector<std::string> tokens;
    for (size_t i = 0; i < content.length(); ) {
        if (std::isspace(content[i])) {
            i++;
            continue;
        }
        if (content[i] == '#') {
            while (i < content.length() && content[i] != '\n') i++;
            continue;
        }
        if (std::string("{};[],").find(content[i]) != std::string::npos) {
            tokens.push_back(std::string(1, content[i]));
            i++;
            continue;
        }
        size_t start = i;
        while (i < content.length() && !std::isspace(content[i]) &&
               std::string("{};[],").find(content[i]) == std::string::npos) {
            i++;
        }
        tokens.push_back(content.substr(start, i - start));
    }
    return tokens;
}

// Main parse function: orchestrates the tokenizing and parsing.
void Config::parse(const std::string& config_file) {
    std::ifstream file(config_file.c_str());
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file: " + config_file);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::vector<std::string> tokens = tokenize(buffer.str());
    std::reverse(tokens.begin(), tokens.end());

    while (!tokens.empty()) {
        if (tokens.back() == "server") {
            tokens.pop_back(); // Consume "server"
            Server server;
            parseServer(server, tokens);
            _servers.push_back(server);
        } else {
            throw std::runtime_error("Unexpected token outside server block: " + tokens.back());
        }
    }
    if (_servers.empty()) {
        throw std::runtime_error("No server blocks found in configuration file.");
    }
	std::cout << "Configuration '" << config_file << "' parsed successfully." << std::endl;
    for (size_t i = 0; i < _servers.size(); ++i) {
        std::cout << "\n=== Server " << i + 1 << " Configuration ===" << std::endl;
        _servers[i].print();
        std::cout << "=================================\n" << std::endl;
    }
}

// Helper to parse array-like values e.g. [GET, POST, HEAD]
static std::vector<std::string> parseArray(std::vector<std::string>& tokens) {
    std::vector<std::string> values;
    if (tokens.empty() || tokens.back() != "[") throw std::runtime_error("Expected '[' for methods array");
    tokens.pop_back(); // consume '['

    while (!tokens.empty() && tokens.back() != "]") {
        if (tokens.back() != ",") {
            values.push_back(tokens.back());
        }
        tokens.pop_back();
    }
    if (tokens.empty() || tokens.back() != "]") throw std::runtime_error("Expected ']' to close methods array");
    tokens.pop_back(); // consume ']'
    return values;
}


// Parses a server block from the token stream.
void Config::parseServer(Server& server, std::vector<std::string>& tokens) {
    if (tokens.empty() || tokens.back() != "{") {
        throw std::runtime_error("Expected '{' after 'server'");
    }
    tokens.pop_back(); // Consume "{"

    while (!tokens.empty() && tokens.back() != "}") {
        std::string directive = tokens.back();
        tokens.pop_back();

        if (directive == "listen") {
            server.setPort(atoi(tokens.back().c_str()));
            tokens.pop_back();
        } else if (directive == "host") {
            server.setHost(tokens.back());
            tokens.pop_back();
        } else if (directive == "server_name") {
            while (!tokens.empty() && tokens.back() != ";") {
                server.addServerName(tokens.back());
                tokens.pop_back();
            }
        } else if (directive == "root") {
            server.setRoot(tokens.back());
            tokens.pop_back();
        } else if (directive == "error_page") {
            std::vector<std::string> values;
            while (!tokens.empty() && tokens.back() != ";") {
                values.push_back(tokens.back());
                tokens.pop_back();
            }
            if (values.size() < 2) throw std::runtime_error("Invalid error_page directive");
            std::string page = values.back();
            values.pop_back();
            for (size_t i = 0; i < values.size(); ++i) {
                server.addErrorPage(atoi(values[i].c_str()), page);
            }
        } else if (directive == "client_max_body_size") {
            server.setClientMaxBodySize(tokens.back());
            tokens.pop_back();
        } else if (directive == "location") {
            Location location;
            location.setPath(tokens.back());
            tokens.pop_back();
            parseLocation(location, tokens);
            server.addLocation(location);
            continue; // location blocks don't have a trailing semicolon
        } else {
            throw std::runtime_error("Unknown server directive: " + directive);
        }

        if (tokens.empty() || tokens.back() != ";") {
            throw std::runtime_error("Expected ';' after directive value for " + directive);
        }
        tokens.pop_back(); // Consume ";"
    }

    if (tokens.empty() || tokens.back() != "}") {
        throw std::runtime_error("Expected '}' to close server block");
    }
    tokens.pop_back(); // Consume "}"
}

// Parses a location block from the token stream.
void Config::parseLocation(Location& location, std::vector<std::string>& tokens) {
    if (tokens.empty() || tokens.back() != "{") {
        throw std::runtime_error("Expected '{' after location path");
    }
    tokens.pop_back(); // Consume "{"

    while (!tokens.empty() && tokens.back() != "}") {
        std::string directive = tokens.back();
        tokens.pop_back();

        if (directive == "root") {
            location.setRoot(tokens.back());
            tokens.pop_back();
        } else if (directive == "methods") {
            std::vector<std::string> methods = parseArray(tokens);
            for(size_t i = 0; i < methods.size(); ++i) {
                location.addAllowedMethod(methods[i]);
            }
        } else if (directive == "index") {
            location.setIndex(tokens.back());
            tokens.pop_back();
            // Also consume other potential index files
            while (!tokens.empty() && tokens.back() != ";") {
                tokens.pop_back();
            }
        } else if (directive == "autoindex") {
            location.setAutoindex(tokens.back() == "on");
            tokens.pop_back();
        } else if (directive == "return") {
            int code = atoi(tokens.back().c_str());
            tokens.pop_back();
            location.setReturn(code, tokens.back());
            tokens.pop_back();
        } else if (directive == "cgi") {
            std::string ext = tokens.back();
            tokens.pop_back();
            location.addCgi(ext, tokens.back());
            tokens.pop_back();
        } else {
            throw std::runtime_error("Unknown location directive: " + directive);
        }

        if (tokens.empty() || tokens.back() != ";") {
            throw std::runtime_error("Expected ';' after location directive value for " + directive);
        }
        tokens.pop_back(); // Consume ";"
    }

    if (tokens.empty() || tokens.back() != "}") {
        throw std::runtime_error("Expected '}' to close location block");
    }
    tokens.pop_back(); // Consume "}"
}

