#ifndef PRINTUTILS_HPP
# define PRINTUTILS_HPP

#include <iostream>
#include <string>
#include <map>
#include "../request/Request.hpp"

// ANSI color codes
#define BLUE "\033[34m"
#define RESET "\033[0m"

class PrintUtils {

	public:

	static void	printBody(const Request& req) {
		std::cout << "--- REQUEST BODY -------" << std::endl;

		if (req.getBody().size() > 200) {
			std::cout << "\n(...message is limited to 200 bytes..))\n";
			std::cout << BLUE << req.getBody().substr(0, 200) << RESET << std::endl;
		}
		else {
			std::cout << BLUE << req.getBody() << RESET << std::endl;

		}
		
		//std::cout << BLUE << req.getBody() << RESET << std::endl;
	}

	static void printStringMap(const std::map<std::string, std::string>& map) {
		for (std::map<std::string, std::string>::const_iterator it = map.begin(); it != map.end(); ++it) {
			std::cout << BLUE << "'" << it->first << "': '";
			std::cout << it->second << "'" << RESET << std::endl;
		}
	}

	static void printRequestHeaders(const Request& req) {
		std::cout << "--- Request Headers -----" << std::endl;
		// Directly accesses the private _headers member of the Request class
		for (std::map<std::string, std::string>::const_iterator it = req._headers.begin(); it != req._headers.end(); ++it) {
			std::cout << BLUE << "'" << it->first << "': '";
			std::cout << it->second << "'" << RESET << std::endl;
		}
		std::cout << "-----------------------" << std::endl;
	}

	static void printRequestLineInfo(const Request& req) {
		std::cout << "--- Request Line Info ---" << std::endl;

		// Convert method enum to string for printing
		std::string methodStr;
		switch (req._method) {
			case Request::GET:    methodStr = "GET"; break;
			case Request::POST:   methodStr = "POST"; break;
			case Request::DELETE: methodStr = "DELETE"; break;
			default:              methodStr = "INVALID"; break;
		}

		std::cout << BLUE << "Method enum: '" << methodStr << "'" << RESET << std::endl;
		std::cout << BLUE << "Method str '" << req.getMethod() << "'" << RESET << std::endl;
		std::cout << BLUE << "URI: '" << req._uri << "'" << RESET << std::endl;
		std::cout << BLUE << "Version: '" << req._httpVersion << "'" << RESET << std::endl;
		std::cout << "-------------------------" << std::endl;
	}

};

# endif