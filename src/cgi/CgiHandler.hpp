#ifndef CGIHANDLER_HPP
# define CGIHANDLER_HPP

# include "../request/Request.hpp"
# include <string>
# include <map>
# include <vector>
# include <cstdlib>
# include <unistd.h>
# include <sys/wait.h>
# include <cstring>
# include <iostream>

# define CGI_DEBUG 1

class CgiHandler {
	public:
		// Pass the request, the full path to the script, and the path to the python interpreter
		CgiHandler(const Request& request, const std::string& scriptPath, const std::string& interpreterPath);
		~CgiHandler();

		// Executes the script and returns the full output (headers + body)
		std::string	executeCgi();

	private:
		const Request&						_request;
		std::string							_scriptPath;
		std::string							_interpreterPath;
		std::map<std::string, std::string>	_env;

		void	setupEnv();
		char**	getEnvArray();
		void	freeEnvArray(char** envArray);
};

#endif
