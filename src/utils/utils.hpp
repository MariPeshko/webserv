#ifndef UTILS_HPP
#define UTILS_HPP

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h> // for uint32_t in C++98

const char*	generateStatusMessage(short status_code);
int			buildHtmlIndexTable(std::string &dir_name, std::string &body, size_t &body_len);
bool		isDirectory(const std::string& path);

template <typename T>
std::string	toString(const T val)
{
	std::stringstream	stream;
	
	stream << val;
	return stream.str();
}

std::string	ipv4_to_string(uint32_t ip);
bool	is_only_digits(const std::string& str);

#endif
