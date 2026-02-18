#ifndef CGIHANDLER_HPP
# define CGIHANDLER_HPP

# include "../request/Request.hpp"
# include "../response/Response.hpp"
# include "../../inc/Webserv.hpp"
# include "signal.h"

# define CGI_DEBUG 0
# define CGI_TIMEOUT 10 // 10 seconds

class Response;

class CgiHandler {
	public:
		// Pass the request, the full path to the script, and the path to the python interpreter
		CgiHandler(Response& resp, const std::string& scriptPath, 
					const std::string& interPath, const std::string& ext);
		~CgiHandler();

		// Executes the script and returns the full output (headers + body)
		std::string	executeCgi();

	private:
		Response&							_resp;
		std::string							_scriptPath;
		std::string							_interpreterPath;
		std::map<std::string, std::string>	_env;
		std::string							_extention; /// with dot .py

		void	setupEnv();
		char**	getEnvArray();
		void	freeEnvArray(char** envArray);

		// executeCgi() helpers
		std::string	writeRequestBodyToCgi(pid_t pid, int pipeInFd, int pipeOutFd, 
									const std::string& body, std::string& preResult);
		std::string	readCgiOutput(pid_t pid, int pipeOutFd, const std::string& preResult);
		std::string	drainRemainingOutput(int pipeOutFd, char* buffer, size_t bufSize);
		void		killAndCleanupCgi(pid_t pid, int pipeIn, int pipeOut);
		ssize_t		drainCgiOutput(int pipeOutFd, std::string& preResult);
		bool		hasChildExited(pid_t pid);
		bool		hasChildExited(pid_t pid, int *status);
};

#endif
