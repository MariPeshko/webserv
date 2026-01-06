#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <iostream>
#include <dirent.h>
#include <sstream>
#include <sys/stat.h>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>

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

#endif
