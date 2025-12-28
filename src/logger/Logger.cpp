#include "Logger.hpp"
#include <sstream>

std::ofstream Logger::_logFile;
std::string Logger::_filename;

void Logger::init(const std::string &filename)
{
	_filename = filename;
	_logFile.open(filename.c_str(), std::ios::app);
	if (!_logFile.is_open())
	{
		std::cerr << "Error: Could not open log file: " << filename << std::endl;
	}
}

void Logger::log(LogLevel level, const std::string &message)
{
	if (!IS_LOGGER_ENABLED)
		return;
	std::string timestamp = getTimestamp();
	std::string levelStr = levelToString(level);
	std::string logMsg = "[" + timestamp + "] [" + levelStr + "] " + message;

	if (_logFile.is_open())
	{
		_logFile << logMsg << std::endl;
	}

	std::string colorCode;
	switch (level)
	{
	case LOG_INFO:
		colorCode = GREEN;
		break;
	case LOG_WARNING:
		colorCode = YELLOW;
		break;
	case LOG_ERROR:
		colorCode = RED;
		break;
	case LOG_REQUEST:
		colorCode = BLUE;
		break;
	default:
		colorCode = RESET;
		break;
	}
	std::cout << colorCode << logMsg << RESET << std::endl;
}

void Logger::logErrno(LogLevel level, const std::string &message)
{
	if (errno != 0)
	{
		std::string errorStr = strerror(errno);
		log(level, message + ": " + errorStr);
	}
	else
	{
		log(level, message);
	}
}

void Logger::logRequest(const std::string &clientIp, const std::string &method, const std::string &uri, int statusCode, size_t bytesSent)
{
	if (!IS_LOGGER_ENABLED)
		return;
	std::string statusCodeColor;
	if (statusCode >= 200 && statusCode < 300)
		statusCodeColor = GREEN;
	else if (statusCode >= 300 && statusCode < 400)
		statusCodeColor = YELLOW;
	else if (statusCode >= 400)
		statusCodeColor = RED;
	std::string timestamp = getTimestamp();
	std::stringstream ss;
	ss << "[" << timestamp << "] [REQUEST] "
	   << clientIp << " - "
	   << "\"" << method << " " << uri << "\" "
	   << statusCodeColor << statusCode << RESET << " "
	   << "- " << bytesSent << " bytes sent";

	std::string logMsg = ss.str();

	if (_logFile.is_open())
	{
		_logFile << logMsg << std::endl;
	}
	std::cout << BLUE << logMsg << RESET << std::endl;
}

std::string Logger::getTimestamp()
{
	std::time_t now = std::time(0);
	char buf[80];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
	return std::string(buf);
}

std::string Logger::levelToString(LogLevel level)
{
	switch (level)
	{
	case LOG_INFO:
		return "INFO";
	case LOG_WARNING:
		return "WARNING";
	case LOG_ERROR:
		return "ERROR";
	case LOG_REQUEST:
		return "REQUEST";
	default:
		return "UNKNOWN";
	}
}
