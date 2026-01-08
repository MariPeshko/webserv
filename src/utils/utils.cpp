#include "utils.hpp"
#include "../response/Response.hpp"

const char *generateStatusMessage(short status_code)
{
	switch (status_code) {
	case 200: return "OK";
	case 201: return "Created";
	case 204: return "No Content";
	case 300: return "Multiple Choices";
	case 301: return "Moved Permanently";
	case 303: return "See Other";
	case 400: return "Bad Request";
	case 401: return "Unauthorized";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 405: return "Method Not Allowed";
	case 409: return "Conflict";
	case 414: return "URI Too Long";
	case 415: return "Unsupported Media Type";
	case 431: return "Request Header Fields Too Large";
	case 500: return "Internal Server Error";
	case 501: return "Not Implemented";
	case 502: return "Bad Gateway";
	default: return "Unknown Status";
	}
}

int	buildHtmlIndexTable(std::string &dir_name, std::string &body, size_t &body_len)
{
	struct dirent*	entityStruct;
	DIR*			directory;
	std::string		dirListPage;

	if (DEBUG) std::cout << GREEN << "Building HTML index for directory: " << dir_name << RESET << std::endl;

	directory = opendir(dir_name.c_str());
	if (directory == NULL) {
		std::cerr << "opendir failed" << std::endl;
		return (1);
	}

	dirListPage.reserve(2048);
	dirListPage.append("<html>\n<head>\n<title> Index of ");
	dirListPage.append(dir_name);
	dirListPage.append("</title>\n</head>\n");
	dirListPage.append("<body>\n<h1> Index of ");
	dirListPage.append(dir_name);
	dirListPage.append("</h1>\n");
	dirListPage.append("<table style=\"width:80%; font-size: 15px\">\n<hr>\n");
	dirListPage.append("<th style=\"text-align:left\"> File Name </th>\n");
	dirListPage.append("<th style=\"text-align:left\"> Last Modification </th>\n");
	dirListPage.append("<th style=\"text-align:left\"> File Size </th>\n");

	struct stat	file_stat;
	std::string	file_path;

	while ((entityStruct = readdir(directory)) != NULL)
	{
		if (strcmp(entityStruct->d_name, ".") == 0)
			continue;

		file_path = dir_name;
		if (!file_path.empty() && file_path[file_path.length() - 1] != '/')
			file_path += "/";
		file_path += entityStruct->d_name;
		if (DEBUG)
			std::cout << YELLOW << "Processing entity: " << file_path << RESET << std::endl;

		if (stat(file_path.c_str(), &file_stat) != 0) {
			if (DEBUG) std::cout << RED << "stat() failed for: " << file_path << RESET << std::endl;
			continue;
		}

		dirListPage.append("<tr>\n<td>\n<a href=\"");
		dirListPage.append(entityStruct->d_name);
		if (S_ISDIR(file_stat.st_mode))
			dirListPage.append("/");
		dirListPage.append("\">");
		dirListPage.append(entityStruct->d_name);
		if (S_ISDIR(file_stat.st_mode))
			dirListPage.append("/");
		dirListPage.append("</a>\n</td>\n<td>\n");
		dirListPage.append(ctime(&file_stat.st_mtime));
		dirListPage.append("</td>\n<td>\n");
		if (!S_ISDIR(file_stat.st_mode))
			dirListPage.append(toString(file_stat.st_size));
		else
			dirListPage.append("-");
		dirListPage.append("</td>\n</tr>\n");
	}
	closedir(directory);

	dirListPage.append("</table>\n<hr>\n</body>\n</html>\n");

	body = dirListPage;
	body_len = body.size();
	return (0);
}

bool	isDirectory(const std::string &path)
{
	struct stat s;
	if (stat(path.c_str(), &s) == 0)
	{
		if (s.st_mode & S_IFDIR)
		{
			return true;
		}
	}
	return false;
}

/** Convert IPv4 address to string manually (C++98 compatible) */
std::string	ipv4_to_string(uint32_t ip) {
	std::ostringstream	oss;
	
	oss << ((ip >> 24) & 0xFF) << "."
		<< ((ip >> 16) & 0xFF) << "."
		<< ((ip >> 8) & 0xFF) << "."
		<< (ip & 0xFF);
	return oss.str();
}

bool	is_only_digits(const std::string& str) {
	if (str.empty())
        return false;
    for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
        if (!std::isdigit(static_cast<unsigned char>(*it)))
            return false;
    }
    return true;
}