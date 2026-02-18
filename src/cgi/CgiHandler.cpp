#include "CgiHandler.hpp"
#include "../server/ServerManager.hpp"

// Forward declaration to access the global ServerManager
extern ServerManager*	g_server_manager;

using std::string;
using std::map;

CgiHandler::CgiHandler(Response& resp, const string& scriptPath, 
						const string& interPath, const string& ext) :
	_resp(resp),
	_scriptPath(scriptPath),
	_interpreterPath(interPath),
	_extention(ext)
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
 * 
 * PATH_INFO: empty unless there is extra path after the script name.
 * For .bla scripts, treat the script as the last path segment.
 * /directory/youpi.bla - PATH_INFO = ""
 * /directory/youpi.bla/foo/bar - PATH_INFO = "/foo/bar"
 * */
void	CgiHandler::setupEnv() {
	const Request*	req = _resp.getRequest();
	string			uri = req->getUri();
	string			uriNoQuery = uri;

	_env["REQUEST_METHOD"] = req->getMethod();
	_env["SCRIPT_FILENAME"] = _scriptPath;
	_env["SERVER_PROTOCOL"] = req->getVersion();
	_env["REDIRECT_STATUS"] = "200"; // Required by some CGI engines (like php-cgi)
	_env["GATEWAY_INTERFACE"] = "CGI/1.1";
	_env["SERVER_SOFTWARE"] = "webserv/1.0";
	_env["SERVER_NAME"] = _resp.getServerConfig().getFirstServerName();
	_env["SERVER_PORT"] = toString(_resp.getServerConfig().getPort());
	_env["REQUEST_URI"] = uri;

	// Parse Query String
	size_t	queryPos = uri.find('?');
	if (queryPos != string::npos) {
		_env["QUERY_STRING"] = uri.substr(queryPos + 1);
		uriNoQuery = uri.substr(0, queryPos);
	} else
		_env["QUERY_STRING"] = "";
	_env["SCRIPT_NAME"] = uriNoQuery;

	// Parse PATH_INFO
	string	pathInfo = "";
	size_t	dot = uriNoQuery.rfind(_extention);
	if (dot != string::npos) {
		size_t	scriptEnd = dot + _extention.size(); // ".bla" or ".py" length
		if (scriptEnd < uriNoQuery.size() && uriNoQuery[scriptEnd] == '/') {
			// extra path follows the script and starts with '/'
			pathInfo = uriNoQuery.substr(scriptEnd);
		} else {
			// note: 42 tester-compat fallback: keep PATH_INFO equal to the script URI (noQuery)
			pathInfo = uriNoQuery;
		}
	} else
		pathInfo = uriNoQuery;
	_env["PATH_INFO"] = pathInfo;

	// Handle Body / Content-Type
	if (!req->getBody().empty()) {
		std::ostringstream	ss;
		ss << req->getBody().length();
		_env["CONTENT_LENGTH"] = ss.str();
		_env["CONTENT_TYPE"] = req->getHeaderValue("content-type");
	}
	
	// Convert headers to HTTP_ format. Convert "User-Agent" to "HTTP_USER_AGENT"
	map<string, string>	headers = req->getHeaders();
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
	if (CGI_DEBUG) {
		std::cout << "SCRIPT_FILENAME: " << _env["SCRIPT_FILENAME"] << std::endl;
		std::cout << "SCRIPT_NAME:     " << _env["SCRIPT_NAME"] << std::endl;
		std::cout << "PATH_INFO:       " << _env["PATH_INFO"] << std::endl;
		std::cout << "QUERY_STRING:    " << _env["QUERY_STRING"] << std::endl;
	}
}

/**
 * Executes a CGI script using fork/exec and captures its output.
 * 
 *  I/O topology:
 *  - pipeIn:  parent writes request body -> child's stdin
 *  - pipeOut: child's stdout -> parent reads CGI output
 *  
 * - Forks a child process to run the CGI script
 * - Child process:
 *    - Redirects stdin/stdout to the pipes
 *    - Closes unused file descriptors to prevent leakage
 *    - Executes the script using execve() with environment variables
 * - Parent process:
 *    - Writes request body to script's stdin
 *    - Reads script's output from stdout
 *    - Waits for child process to complete
 * 
 * Why do we need O_NONBLOCK - it prevents deadlock
 * Deadlock note: pipe buffer is only 64KB, so after 64KB, write() BLOCKS (waits).
 * Meanwhile, the CGI script fills its output pipe (64KB) - BLOCKS (pipe full)
 * Deadlock: Parent waiting for child to read, child waiting for parent to read
 * 
 * Once child has exited: Switch to blocking. No more deadlock risk because
 * child can't block anymore and reading from a closed pipe will reach EOF.
 * 
 * - Timeout handling
 * 
 * @return The complete output from the CGI script (headers + body),
 *         or an error message string if execution fails
 */
string	CgiHandler::executeCgi()
{
	const Request*	req = _resp.getRequest();
	int				pipeIn[2];  // To send Body to script
	int				pipeOut[2]; // To read Output from script

	if (pipe(pipeIn) == -1 || pipe(pipeOut) == -1)
		throw std::runtime_error("CGI: pipe() failed");

	pid_t	pid = fork();
	if (pid == -1)
		throw std::runtime_error("CGI: pipe() failed");

	if (pid == 0) { // Child Process
		close(pipeIn[1]);
		close(pipeOut[0]);
		dup2(pipeIn[0], STDIN_FILENO);
		dup2(pipeOut[1], STDOUT_FILENO);
		close(pipeIn[0]);
		close(pipeOut[1]);

		// Close all other file descriptors to prevent leakage
		int	max_fd = sysconf(_SC_OPEN_MAX);
		if (max_fd == -1) max_fd = 1024; // Fallback if sysconf fails
		for (int i = 3; i < max_fd; ++i)
			close(i);

		char**	env = getEnvArray();
		char*	argv[] = {
			const_cast<char*>(_interpreterPath.c_str()),
			const_cast<char*>(_scriptPath.c_str()),
			NULL
		};

		execve(_interpreterPath.c_str(), argv, env);
		std::cerr << "Execve failed" << std::endl;
		freeEnvArray(env);
		exit(1);
	} else { // Parent Process
		close(pipeIn[0]);
		close(pipeOut[1]);

		// For a a CGI timeout: Non-blocking I/O on pipes
		fcntl(pipeIn[1], F_SETFL, O_NONBLOCK);
		fcntl(pipeOut[0], F_SETFL, O_NONBLOCK);

		// For a a CGI timeout: Write request body (best-effort, non-blocking, with timeout).
		// write() system call is not guaranteed to send all your data in one go;
		// We need it for a 42_test POST http://localhost:8080/directory/youpi.bla with a size of 100000000
		const string&	body = req->getBody();
		string			preResult;

		string	writeError = writeRequestBodyToCgi(pid, pipeIn[1], pipeOut[0], body, preResult);
		if (!writeError.empty()) {
			return writeError;
		}
		
		close(pipeIn[1]); // Done writing (or child closed early)
		
		string	result = readCgiOutput(pid, pipeOut[0], preResult);
		if (CGI_DEBUG) std::cout << "CGI output size: " << result.size() << std::endl;
		return result;
	}
}

/** Write request body to CGI stdin with timeout and deadlock prevention */
string  	CgiHandler::writeRequestBodyToCgi(pid_t pid, int pipeInFd, int pipeOutFd, 
								   const string& body, string& preResult) {
	size_t			written = 0;
	time_t			lastProgress = time(NULL);
	const time_t	CGI_TIMEOUT_SEC = CGI_TIMEOUT;

	while (written < body.size()) {
		if (g_server_manager && g_server_manager->isShutdownRequested()) {
			killAndCleanupCgi(pid, pipeInFd, pipeOutFd);
			return "Status: 503\r\n\r\nServer Shutting Down";
		}

		if (time(NULL) - lastProgress >= CGI_TIMEOUT_SEC) {
			killAndCleanupCgi(pid, pipeInFd, pipeOutFd);
			if (CGI_DEBUG) std::cout << "CGI Script Timeout (write)" << std::endl;
			return "Status: 504\r\n\r\nCGI Script Timeout (write)";
		}

		ssize_t n = write(pipeInFd, body.c_str() + written, body.size() - written);
		if (n > 0) {
			written += static_cast<size_t>(n);
			lastProgress = time(NULL);
			continue;
		}

		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				ssize_t r = drainCgiOutput(pipeOutFd, preResult);
				if (r > 0) lastProgress = time(NULL);
				if (hasChildExited(pid)) break;
				continue;
			}
			killAndCleanupCgi(pid, pipeInFd, pipeOutFd);
			return "Status: 500\r\n\r\nCGI Write Error";
		}

		// n == 0
		ssize_t r = drainCgiOutput(pipeOutFd, preResult);
		if (r > 0)
			lastProgress = time(NULL);
		if (hasChildExited(pid))
			break;
	}
	if (CGI_DEBUG) std::cout << "request body is sent to cgi. written counter: " << written << std::endl;
	return ""; // Success, no error
}

/** Read CGI output with timeout */
std::string	CgiHandler::readCgiOutput(pid_t pid, int pipeOutFd, const string& preResult) {
	char			buffer[65536];
	string			result = preResult;
	time_t			lastProgress = time(NULL);
	const time_t	CGI_TIMEOUT_SEC = CGI_TIMEOUT;

	while (true) {
		if (g_server_manager && g_server_manager->isShutdownRequested()) {
			killAndCleanupCgi(pid, -1, pipeOutFd);
			return "Status: 503\r\n\r\nServer Shutting Down";
		}

		if (time(NULL) - lastProgress >= CGI_TIMEOUT_SEC) {
			killAndCleanupCgi(pid, -1, pipeOutFd);
			if (CGI_DEBUG) std::cout << "CGI Script Timeout (read)" << std::endl;
			return "Status: 504\r\n\r\nCGI Script Timeout";
		}

		ssize_t	n = read(pipeOutFd, buffer, sizeof(buffer));
		if (n > 0) {
			result.append(buffer, static_cast<size_t>(n));
			lastProgress = time(NULL);
			continue;
		}

		int	status;
		if (hasChildExited(pid, &status)) {
			// Switch to blocking and drain remaining
			int flags = fcntl(pipeOutFd, F_GETFL, 0);
			if (flags != -1) {
				fcntl(pipeOutFd, F_SETFL, flags & ~O_NONBLOCK);
			}

			string	remaining = drainRemainingOutput(pipeOutFd, buffer, sizeof(buffer));
			result.append(remaining);

			close(pipeOutFd);

			if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
				if (CGI_DEBUG) std::cout << "executeCgi(): Child process failed" << std::endl;
				return "Status: 500\r\n\r\nCGI Script Error";
			}
			if (WIFSIGNALED(status)) {
				if (CGI_DEBUG) std::cout << "executeCgi(): Child terminated by signal" << std::endl;
				return "Status: 500\r\n\r\nCGI Script Terminated";
			}

			if (CGI_DEBUG) std::cout << "CGI output size: " << result.size() << std::endl;
			return result;
		}
	}
}

/** Drain remaining output from CGI after child exits (blocking mode) */
string	CgiHandler::drainRemainingOutput(int pipeOutFd, char* buffer, size_t bufSize)
{
	string	remaining;
	while (true) {
		ssize_t n2 = read(pipeOutFd, buffer, bufSize);
		if (n2 > 0) {
			remaining.append(buffer, static_cast<size_t>(n2));
		} else if (n2 == 0) {
			break; // EOF: fully drained
		} else { // n2 < 0: if interrupted, retry; otherwise stop
			if (errno == EINTR) continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
			break;
		}
	}
	return remaining;
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

void	CgiHandler::killAndCleanupCgi(pid_t pid, int pipeIn, int pipeOut) {
	kill(pid, SIGKILL);
	if (pipeIn != -1)
		close(pipeIn);
	if (pipeOut != -1)
		close(pipeOut);

	int	status;
	waitpid(pid, &status, 0);
}

ssize_t	CgiHandler::drainCgiOutput(int pipeOutFd, string& preResult) {
	char	drainBuf[4096];
	ssize_t	r = read(pipeOutFd, drainBuf, sizeof(drainBuf));
	if (r > 0) {
		preResult.append(drainBuf, static_cast<size_t>(r));
	}
	return r;
}

// Check if child already exited
// WNOHANG - return immediately if no child has exited.
// If a child died, its pid will be returned by waitpid().
// If nothing died, then the returned pid is 0.
bool	CgiHandler::hasChildExited(pid_t pid) {
	int		status = 0;
	pid_t	rv = waitpid(pid, &status, WNOHANG);
	return (rv == pid);
}

// Similar method with a pointer to a status
bool	CgiHandler::hasChildExited(pid_t pid, int *status) {
	pid_t	rv = waitpid(pid, status, WNOHANG);
	return (rv == pid);
}

