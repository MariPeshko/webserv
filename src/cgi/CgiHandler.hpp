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
		CgiHandler(Response& resp, const std::string& scriptPath, const std::string& interpreterPath);
		~CgiHandler();

		// Executes the script and returns the full output (headers + body)
		std::string	executeCgi();

	private:
		Response&							_resp;
		std::string							_scriptPath;
		std::string							_interpreterPath;
		std::map<std::string, std::string>	_env;

		void	setupEnv();
		char**	getEnvArray();
		void	freeEnvArray(char** envArray);
};

#endif
