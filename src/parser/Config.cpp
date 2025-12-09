#include "Config.hpp"
#include <iostream>
#include <algorithm>
#include <set>

Config::Config() {}
Config::~Config() {}

std::vector<Server> &Config::getServerConfigs()
{
	return _servers;
}

// Tokenizer: Converts the raw configuration string into a vector of tokens.
std::vector<std::string> Config::tokenize(const std::string &content)
{
	std::vector<std::string> tokens;
	for (size_t i = 0; i < content.length();)
	{
		// Skip whitespace
		if (std::isspace(content[i]))
		{
			i++;
			continue;
		}
		// Skip comments
		if (content[i] == '#')
		{
			while (i < content.length() && content[i] != '\n')
				i++;
			continue;
		}
		// Handle special characters as separate tokens
		if (std::string("{};[],").find(content[i]) != std::string::npos)
		{
			tokens.push_back(std::string(1, content[i]));
			i++;
			continue;
		}
		// Parse words/values
		size_t start = i;
		while (i < content.length() && !std::isspace(content[i]) &&
			   std::string("{};[],").find(content[i]) == std::string::npos)
		{
			i++;
		}
		tokens.push_back(content.substr(start, i - start));
	}
	return tokens;
}

// Main parse function: orchestrates the tokenizing and parsing.
void Config::parse(const std::string &config_file)
{
	std::ifstream file(config_file.c_str());
	if (!file.is_open())
	{
		throw std::runtime_error("Could not open config file: " + config_file);
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	file.close();

	std::vector<std::string> tokens = tokenize(buffer.str());
	std::reverse(tokens.begin(), tokens.end());

	while (!tokens.empty())
	{
		if (tokens.back() == "server")
		{
			tokens.pop_back(); // Consume "server"
			Server server;
			parseServer(server, tokens);
			_servers.push_back(server);
		}
		else
		{
			throw std::runtime_error("Unexpected token outside server block: " + tokens.back());
		}
	}
	if (_servers.empty())
	{
		throw std::runtime_error("No server blocks found in configuration file.");
	}
	/* std::cout << "Configuration '" << config_file << "' parsed successfully." << std::endl;
	for (size_t i = 0; i < _servers.size(); ++i) {
		std::cout << "\n=== Server " << i + 1 << " Configuration ===" << std::endl;
		_servers[i].print();
		std::cout << "=================================\n" << std::endl;
	} */
}

// Helper to check if a token is a known directive (to handle missing semicolons)
static bool isDirective(const std::string &token)
{
	static const char *directives[] = {
		"listen", "host", "server_name", "root", "error_page", "client_max_body_size",
		"client_body_buffer_size", "location", "methods", "allow_methods", "index",
		"autoindex", "return", "cgi", "cgi_pass", "alias", "}"};
	for (size_t i = 0; i < 17; ++i)
	{
		if (token == directives[i])
			return true;
	}
	return false;
}

// Helper to consume optional semicolon
static void consumeSemiColon(std::vector<std::string> &tokens)
{
	if (!tokens.empty() && tokens.back() == ";")
	{
		tokens.pop_back();
	}
}

// Helper to parse array-like values e.g. [GET, POST, HEAD] or simple list GET POST
static std::vector<std::string> parseValues(std::vector<std::string> &tokens)
{
	std::vector<std::string> values;

	if (tokens.empty())
		return values;

	// Handle bracket format [ GET, POST ]
	if (tokens.back() == "[")
	{
		tokens.pop_back(); // consume '['
		while (!tokens.empty() && tokens.back() != "]")
		{
			if (tokens.back() != ",")
			{
				values.push_back(tokens.back());
			}
			tokens.pop_back();
		}
		if (tokens.empty() || tokens.back() != "]")
			throw std::runtime_error("Expected ']'");
		tokens.pop_back(); // consume ']'
	}
	else
	{
		// Handle space separated format until semicolon or next directive
		while (!tokens.empty() && tokens.back() != ";" && !isDirective(tokens.back()))
		{
			values.push_back(tokens.back());
			tokens.pop_back();
		}
	}
	return values;
}

// Parses a server block from the token stream.
void Config::parseServer(Server &server, std::vector<std::string> &tokens)
{
	if (tokens.empty() || tokens.back() != "{")
	{
		throw std::runtime_error("Expected '{' after 'server'");
	}
	tokens.pop_back(); // Consume "{"

	while (!tokens.empty() && tokens.back() != "}")
	{
		std::string directive = tokens.back();
		tokens.pop_back();

		if (directive == "listen")
		{
			std::string val = tokens.back();
			tokens.pop_back();
			size_t colonPos = val.find(':');
			if (colonPos != std::string::npos)
			{
				server.setHost(val.substr(0, colonPos));
				server.setPort(atoi(val.substr(colonPos + 1).c_str()));
			}
			else
			{
				server.setPort(atoi(val.c_str()));
			}
		}
		else if (directive == "host")
		{
			server.setHost(tokens.back());
			tokens.pop_back();
		}
		else if (directive == "server_name")
		{
			std::vector<std::string> names = parseValues(tokens);
			for (size_t i = 0; i < names.size(); ++i)
			{
				server.addServerName(names[i]);
			}
		}
		else if (directive == "root")
		{
			server.setRoot(tokens.back());
			tokens.pop_back();
		}
		else if (directive == "index")
		{
			std::vector<std::string> indices = parseValues(tokens);
			if (!indices.empty())
				server.setIndex(indices[0]);
		}
		else if (directive == "methods" || directive == "allow_methods")
		{
			std::vector<std::string> methods = parseValues(tokens);
			for (size_t i = 0; i < methods.size(); ++i)
			{
				server.addAllowedMethod(methods[i]);
			}
		}
		else if (directive == "error_page")
		{
			std::vector<std::string> values = parseValues(tokens);
			if (values.size() < 2)
				throw std::runtime_error("Invalid error_page directive");
			std::string page = values.back();
			values.pop_back();
			for (size_t i = 0; i < values.size(); ++i)
			{
				server.addErrorPage(atoi(values[i].c_str()), page);
			}
		}
		else if (directive == "client_max_body_size" || directive == "client_body_buffer_size")
		{
			server.setClientMaxBodySize(tokens.back());
			tokens.pop_back();
		}
		else if (directive == "location")
		{
			Location location;
			location.setPath(tokens.back());
			tokens.pop_back();
			parseLocation(location, tokens, server);
			server.addLocation(location);
			continue; // location blocks don't have a trailing semicolon
		}
		else
		{
			throw std::runtime_error("Unknown server directive: " + directive);
		}
		consumeSemiColon(tokens);
	}

	if (tokens.empty() || tokens.back() != "}")
	{
		throw std::runtime_error("Expected '}' to close server block");
	}
	tokens.pop_back(); // Consume "}"
}

// Parses a location block from the token stream.
void Config::parseLocation(Location &location, std::vector<std::string> &tokens, const Server &server)
{
	if (tokens.empty() || tokens.back() != "{")
	{
		throw std::runtime_error("Expected '{' after location path");
	}
	tokens.pop_back(); // Consume "{"

	while (!tokens.empty() && tokens.back() != "}")
	{
		std::string directive = tokens.back();
		tokens.pop_back();

		if (directive == "root")
		{
			location.setRoot(tokens.back());
			tokens.pop_back();
		}
		else if (directive == "methods" || directive == "allow_methods")
		{
			std::vector<std::string> methods = parseValues(tokens);
			for (size_t i = 0; i < methods.size(); ++i)
			{
				location.addAllowedMethod(methods[i]);
			}
		}
		else if (directive == "index")
		{
			std::vector<std::string> indices = parseValues(tokens);
			if (!indices.empty())
				location.setIndex(indices[0]); // Taking the first one for now, or join them?
		}
		else if (directive == "autoindex")
		{
			location.setAutoindex(tokens.back() == "on");
			tokens.pop_back();
		}
		else if (directive == "return")
		{
			int code = atoi(tokens.back().c_str());
			tokens.pop_back();
			location.setReturn(code, tokens.back());
			tokens.pop_back();
		}
		else if (directive == "cgi")
		{
			std::string ext = tokens.back();
			tokens.pop_back();
			location.addCgi(ext, tokens.back());
			tokens.pop_back();
		}
		else if (directive == "cgi_pass")
		{
			// Assume path extension derived from location path or generic
			std::string path = tokens.back();
			tokens.pop_back();
			// If location path is an extension (e.g. *.php), use it
			std::string locPath = location.getPath();
			if (locPath.size() > 1 && locPath[0] == '*' && locPath[1] == '.')
			{
				location.addCgi(locPath.substr(1), path); // Remove *
			}
			else
			{
				location.addCgi("*", path); // Generic CGI
			}
		}
		else if (directive == "alias")
		{
			// Alias might be empty in mac.conf (just 'alias'?) or have a path
			if (!tokens.empty() && !isDirective(tokens.back()) && tokens.back() != "}")
			{
				location.setAlias(tokens.back());
				tokens.pop_back();
			}
			else
			{
				location.setAlias("on"); // Treat as flag if no value?
			}
		}
		else if (directive == "client_max_body_size" || directive == "client_body_buffer_size")
		{
			location.setClientMaxBodySize(tokens.back());
			tokens.pop_back();
		}
		else if (directive == "location")
		{
			// Nested location
			Location nestedLoc;
			nestedLoc.setPath(tokens.back());
			tokens.pop_back();
			parseLocation(nestedLoc, tokens, server);
			location.addLocation(nestedLoc);
			continue;
		}
		else
		{
			throw std::runtime_error("Unknown location directive: " + directive);
		}
		consumeSemiColon(tokens);
	}

	if (tokens.empty() || tokens.back() != "}")
	{
		throw std::runtime_error("Expected '}' to close location block");
	}
	tokens.pop_back(); // Consume "}"
}
