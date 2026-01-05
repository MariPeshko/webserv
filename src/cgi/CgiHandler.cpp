#include "CgiHandler.hpp"
#include <sstream>
#include <cstdio>
#include <cctype> // Required for std::toupper

using std::string;
using std::map;

CgiHandler::CgiHandler(const Request& request, const string& scriptPath, 
						const string& interpreterPath) :
	_request(request),
	_scriptPath(scriptPath),
	_interpreterPath(interpreterPath)
{
	setupEnv();
}

CgiHandler::~CgiHandler() {}

/** 
 * Info: CGI scripts are separate programs that our web server executes. 
 * They don't have direct access to the HTTP request data. The CGI 
 * specification defines that request information must be passed 
 * via environment variables.
 * 
 * execve() passes them in executeCgi() method
 * 
 * HTTP Headers Conversion because CGI standard requires HTTP headers to be prefixed 
 * with HTTP_ and use underscores.
 * */
void	CgiHandler::setupEnv() {
	_env["REQUEST_METHOD"] = _request.getMethod();
	_env["SCRIPT_FILENAME"] = _scriptPath;
	_env["SERVER_PROTOCOL"] = _request.getVersion();
	_env["REDIRECT_STATUS"] = "200"; // Required by some CGI engines (like php-cgi)

	/* // Add these missing variables:
    _env["GATEWAY_INTERFACE"] = "CGI/1.1";
    _env["SERVER_SOFTWARE"] = "webserv/1.0";
    // _env["SERVER_NAME"] = // From Host header or config
    // _env["SERVER_PORT"] = // From socket or config
    // _env["REMOTE_ADDR"] = // Client IP
    // _env["REMOTE_HOST"] = // Client hostname (optional)
    _env["SCRIPT_NAME"] = _request.getUri().substr(0, _request.getUri().find('?')); */

	// Parse Query String
	string	uri = _request.getUri();
	size_t	queryPos = uri.find('?');
	if (queryPos != string::npos) {
		if (CGI_DEBUG) std::cout << "CGI: QUERY_STRING: " << uri.substr(queryPos + 1) << std::endl;
		_env["QUERY_STRING"] = uri.substr(queryPos + 1);
		_env["PATH_INFO"] = uri.substr(0, queryPos);
	} else {
		if (CGI_DEBUG) std::cout << "CGI: QUERY_STRING is empty string" << std::endl;
		_env["QUERY_STRING"] = "";
		_env["PATH_INFO"] = uri;
	}

	// Handle Body / Content-Type
	if (!_request.getBody().empty()) {
		std::ostringstream	ss;

		ss << _request.getBody().length();
		_env["CONTENT_LENGTH"] = ss.str();
		_env["CONTENT_TYPE"] = _request.getHeaderValue("content-type");
	}
	
	// Convert headers to HTTP_ format. Convert "User-Agent" to "HTTP_USER_AGENT"
	map<string, string>	headers = _request.getHeaders();
	for (map<string, string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
		string	key = it->first;
		string	value = it->second;
		string	envKey = "HTTP_";

		for (size_t i = 0; i < key.length(); ++i) {
			if (key[i] == '-')
				envKey += '_';
			else
				envKey += std::toupper(key[i]);
		}
		_env[envKey] = value;
	}
}

char**	CgiHandler::getEnvArray()
{
	char**	env = new char*[_env.size() + 1];
	size_t	i = 0;
	for (map<string, string>::const_iterator it = _env.begin(); it != _env.end(); ++it)
	{
		string	element = it->first + "=" + it->second;
		env[i] = new char[element.size() + 1];
		std::strcpy(env[i], element.c_str());
		i++;
	}
	env[i] = NULL;
	return env;
}

void	CgiHandler::freeEnvArray(char** envArray)
{
	if (!envArray) return;

	for (size_t i = 0; envArray[i]; ++i) {
		delete[] envArray[i];
	}
	delete[] envArray;
}

string	CgiHandler::executeCgi()
{
	int		pipeIn[2];  // To send Body to script
	int		pipeOut[2]; // To read Output from script

	if (pipe(pipeIn) == -1 || pipe(pipeOut) == -1) {
		return "Status: 500\r\n\r\nInternal Server Error (Pipe)";
	}

	pid_t	pid = fork();
	if (pid == -1) {
		return "Status: 500\r\n\r\nInternal Server Error (Fork)";
	}

	if (pid == 0) { // Child Process
		close(pipeIn[1]); // Close write end of input pipe
		close(pipeOut[0]); // Close read end of output pipe

		// Redirect stdin to read from pipeIn
		dup2(pipeIn[0], STDIN_FILENO);
		
		// Redirect stdout to write to pipeOut
		dup2(pipeOut[1], STDOUT_FILENO);

		close(pipeIn[0]);
		close(pipeOut[1]);

		// Close all other file descriptors to prevent leakage
		int	max_fd = sysconf(_SC_OPEN_MAX);
		if (max_fd == -1) max_fd = 1024; // Fallback if sysconf fails
		for (int i = 3; i < max_fd; ++i) {
			close(i);
		}

		char**	env = getEnvArray();
		char*	argv[] = {
			const_cast<char*>(_interpreterPath.c_str()),
			const_cast<char*>(_scriptPath.c_str()),
			NULL
		};

		execve(_interpreterPath.c_str(), argv, env);
		
		// If execve fails
		std::cerr << "Execve failed" << std::endl;
		freeEnvArray(env);
		exit(1);
	} else { // Parent Process
		close(pipeIn[0]); // Close read end of input pipe
		close(pipeOut[1]); // Close write end of output pipe

		// Write request body to the script's stdin
		if (!_request.getBody().empty()) {
			write(pipeIn[1], _request.getBody().c_str(), _request.getBody().length());
		}
		close(pipeIn[1]); // Finished writing

		// Read script's stdout
		string	result;
		char		buffer[4096];
		ssize_t		bytesRead;
		while ((bytesRead = read(pipeOut[0], buffer, sizeof(buffer))) > 0) {
			result.append(buffer, bytesRead);
		}
		close(pipeOut[0]);
		
		int	status;
		waitpid(pid, &status, 0); // Wait for child
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
			if (CGI_DEBUG) std::cout << "executeCgi(): Child process failed" << std::endl;
    		return "Status: 500\r\n\r\nCGI Script Error";
		}
		return result;
	}
}
