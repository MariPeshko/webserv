#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <ctime>
#include <cerrno>
#include <cstring>

#define IS_LOGGER_ENABLED 1
// colors
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"

enum LogLevel
{
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR,
	LOG_REQUEST
};

class Logger
{
public:
	static void init(const std::string &filename);
	static void log(LogLevel level, const std::string &message);
	static void logErrno(LogLevel level, const std::string &message);
	static void logRequest(const std::string &clientIp, const std::string &method, const std::string &uri, int statusCode, size_t bytesSent);

private:
	Logger();
	Logger(const Logger &);
	Logger &operator=(const Logger &);

	static std::ofstream _logFile;
	static std::string _filename;

	static std::string getTimestamp();
	static std::string levelToString(LogLevel level);
};

#endif
