#ifndef PRINTUTILS_HPP
# define PRINTUTILS_HPP

#include <iostream>
#include <string>
#include <map>

// ANSI color codes
#define BLUE "\033[34m"
#define RESET "\033[0m"

class PrintUtils {

	public:

	/**
	 * @brief Prints the key-value pairs of a std::map<std::string, std::string> to std::cout.
	 * 
	 * @param map The map to be printed.
	 */
	static void printStringMap(const std::map<std::string, std::string>& map) {
		for (std::map<std::string, std::string>::const_iterator it = map.begin(); it != map.end(); ++it) {
			std::cout << BLUE << "'" << it->first << "': '";
			std::cout << it->second << "'" << RESET << std::endl;
		}
	}

};

# endif