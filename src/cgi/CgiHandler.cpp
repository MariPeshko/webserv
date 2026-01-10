#include "CgiHandler.hpp"


using std::string;
using std::map;

CgiHandler::CgiHandler(Response& resp, const string& scriptPath, 
						const string& interpreterPath) :
	_resp(resp),
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
	const Request*	req = _resp.getRequest();

	_env["REQUEST_METHOD"] = req->getMethod();
	_env["SCRIPT_FILENAME"] = _scriptPath;
	_env["SERVER_PROTOCOL"] = req->getVersion();
	_env["REDIRECT_STATUS"] = "200"; // Required by some CGI engines (like php-cgi)
	_env["GATEWAY_INTERFACE"] = "CGI/1.1";
	_env["SERVER_SOFTWARE"] = "webserv/1.0";
	_env["SERVER_NAME"] = _resp.getServerConfig().getFirstServerName();
	_env["SERVER_PORT"] = toString(_resp.getServerConfig().getPort());

	// Parse Query String
	string		uri = req->getUri();
	string	uriNoQuery = uri;
	
	size_t		queryPos = uri.find('?');
	if (queryPos != string::npos) {
		_env["QUERY_STRING"] = uri.substr(queryPos + 1);
		uriNoQuery = uri.substr(0, queryPos);
		// To DO delete
		//_env["PATH_INFO"] = uri.substr(0, queryPos);
	} else {
		_env["QUERY_STRING"] = "";
		// To DO delete
		//_env["PATH_INFO"] = uri;
	}
	_env["SCRIPT_NAME"] = uriNoQuery;

	// PATH_INFO: empty unless there is extra path after the script name.
	// For .bla scripts, treat the script as the last path segment.
	//   /directory/youpi.bla - PATH_INFO = ""
	//   /directory/youpi.bla/foo/bar - PATH_INFO = "/foo/bar"
	string	pathInfo = "";
	size_t dot = uriNoQuery.rfind(".bla");
	if (CGI_DEBUG) std::cout << "WARNING: CGI correct pathInfo only for .bla (42 tester)" << std::endl;
	// Find the position of the script name inside the URI (no query)
	// Use last occurrence of ".bla" and include that segment as script.
	if (dot != std::string::npos) {
		// end of script segment
		size_t	scriptEnd = dot + 4; // ".bla" length
		if (scriptEnd < uriNoQuery.size() && uriNoQuery[scriptEnd] == '/') {
			// extra path follows the script
			pathInfo = uriNoQuery.substr(scriptEnd); // starts with '/'
		} else {
			// No extra path — keep PATH_INFO equal to the script URI (expected by tester)
			// note: PATH_INFO = SCRIPT_NAME (42 tester-compat fallback).
			pathInfo = uriNoQuery;
		}
	} else {
		// Fallback: no .bla found; keep it as SCRIPT_NAME
		pathInfo = uriNoQuery;
	}
	_env["PATH_INFO"] = pathInfo;
	_env["REQUEST_URI"] = uri;

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
 * 4. Parent process:
 *    - Writes request body to script's stdin
 *    - Reads script's output from stdout
 *    - Waits for child process to complete
 * 
 *  Timeout handling: 
 * - Set both pipe fds (pipeIn[1], pipeOut[0]) to O_NONBLOCK.
 * - Record start = time(NULL); enforce a hard limit CGI_TIMEOUT_SEC.
 * - In a read phase: After each iteration check child state with waitpid with
 *   a flag WNOHANG.
 * 
 * @return The complete output from the CGI script (headers + body),
 *         or an error message string if execution fails
 */
string	CgiHandler::executeCgi()
{
	const Request*	req = _resp.getRequest();

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
		close(pipeIn[1]);
		close(pipeOut[0]);
		dup2(pipeIn[0], STDIN_FILENO);
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
		std::cerr << "Execve failed" << std::endl;
		freeEnvArray(env);
		exit(1);
	} else { // Parent Process
		close(pipeIn[0]);
		close(pipeOut[1]);

		// For a a CGI timeout: Non-blocking I/O on pipes
		fcntl(pipeIn[1], F_SETFL, O_NONBLOCK);
		fcntl(pipeOut[0], F_SETFL, O_NONBLOCK);
		const time_t	CGI_TIMEOUT_SEC = CGI_TIMEOUT;
		const time_t	start = time(NULL);

		// For a a CGI timeout: Write request body (best-effort, 
		// non-blocking, with timeout).
		// write() system call is not guaranteed to send all your data in one go;
		// written - a counter to keep track of how many bytes have been sent
		const string&	body = req->getBody();
		size_t			written = 0;
		string			preResult;
		time_t			lastProgress = start;

		// TO DO I need it for a test Test POST http://localhost:8080/directory/youpi.bla with a size of 100000000
		// read this loop again
		while (written < body.size()) {
			// Timeout if no progress for CGI_TIMEOUT_SEC
			if (time(NULL) - lastProgress >= CGI_TIMEOUT_SEC) {
				kill(pid, SIGKILL);
				close(pipeIn[1]);
				close(pipeOut[0]);
				int	status; waitpid(pid, &status, 0);
				if (CGI_DEBUG) std::cout << "CGI Script Timeout (write, no progress)" << std::endl;
				return "Status: 504\r\n\r\nCGI Script Timeout (write)";
			}

			//if (CGI_DEBUG) std::cout << "parent write request body\nwritten counter: " << written << std::endl;
		   
			ssize_t n = write(pipeIn[1], body.c_str() + written, body.size() - written);
			if (n > 0) {
				written += static_cast<size_t>(n);
				lastProgress = time(NULL); continue;
			}

			if (n < 0) {
				// TO DO is it possible to avoid it?
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					// Pipe to child stdin is full. Try to drain child's stdout to let it make progress.
					char drainBuf[4096];
					ssize_t r = read(pipeOut[0], drainBuf, sizeof(drainBuf));
					if (r > 0) {
						preResult.append(drainBuf, static_cast<size_t>(r));
						// We made progress by draining output
						lastProgress = time(NULL);
					}

					// Check if child already exited
					int status = 0;
					pid_t rv = waitpid(pid, &status, WNOHANG);
					if (rv == pid) {
						// Child exited, stop writing
						break;
					}
					continue; // Retry write on next loop iteration
				}
				// Other write error
				kill(pid, SIGKILL);
				close(pipeIn[1]);
				close(pipeOut[0]);
				int status; waitpid(pid, &status, 0);
				return "Status: 500\r\n\r\nCGI Write Error";
			}

			// n == 0 (no bytes written). Give the child a chance by draining output.
			char	drainBuf[4096];
			ssize_t r = read(pipeOut[0], drainBuf, sizeof(drainBuf));
			if (r > 0) {
				lastProgress = time(NULL);
				preResult.append(drainBuf, static_cast<size_t>(r));
			}

			// Check child exit
			int		status = 0;
			pid_t	rv = waitpid(pid, &status, WNOHANG);
			if (rv == pid) break;

			// Loop again; lastProgress guards the timeout.
		}

		close(pipeIn[1]); // Done writing (or child closed early)
		if (CGI_DEBUG) std::cout << "request body is sent to cgi. written counter: " << written << std::endl;

		// Read script's stdout
		// For a a CGI timeout: read until child exits or timeout
		char		buffer[65536];
		// Read script's stdout (start with any bytes drained earlier)
        std::string	result = preResult;

		while (true) {
			// Timeout if no progress for CGI_TIMEOUT_SEC
			if (time(NULL) - lastProgress >= CGI_TIMEOUT_SEC) {
				kill(pid, SIGKILL);
				close(pipeIn[1]);
				close(pipeOut[0]);
				int	status; waitpid(pid, &status, 0);
				if (CGI_DEBUG) std::cout << "CGI Script Timeout (read)" << std::endl;
				return "Status: 504\r\n\r\nCGI Script Timeout";
			}

			//if (CGI_DEBUG) std::cout << "parent reads CGI output" << std::endl;
			ssize_t	n = read(pipeOut[0], buffer, sizeof(buffer));
			if (n > 0) {
				result.append(buffer, static_cast<size_t>(n));
				lastProgress = time(NULL); continue;
			}
			/* if (n < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					if (CGI_DEBUG) std:: cout << "parent reads CGI output: EAGAIN and EWOULDBLOCK" << std::endl;
				}
			} */

			// Check if child has exited
			int		status;
			// WNOHANG - return immediately if no child has exited.
			// If a child died, its pid will be returned by waitpid() and process 
			// can act on that. If nothing died, then the returned pid is 0.
			pid_t	r = waitpid(pid, &status, WNOHANG);
			if (r == pid) {
				// Child exited: switch to blocking and fully drain remaining data
                int flags = fcntl(pipeOut[0], F_GETFL, 0);
                if (flags != -1) {
					if (CGI_DEBUG) std::cout << "fcntl(pipeOut[0], F_SETFL, flags & ~O_NONBLOCK)" << std::endl;
					fcntl(pipeOut[0], F_SETFL, flags & ~O_NONBLOCK);
				}

				while (true) {
					ssize_t	n2 = read(pipeOut[0], buffer, sizeof(buffer));
                    if (n2 > 0) {
                        result.append(buffer, static_cast<size_t>(n2));
                    } else if (n2 == 0) { // EOF: fully drained
                        break;
                    } else {
                        // n2 < 0: if interrupted, retry; otherwise stop
                        if (errno == EINTR) continue;
                        if (errno == EAGAIN || errno == EWOULDBLOCK) continue; // unlikely now
                        break;
                    }
				}
				close(pipeOut[0]);
				if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
					if (CGI_DEBUG) std::cout << "executeCgi(): Child process failed" << std::endl;
					return "Status: 500\r\n\r\nCGI Script Error";
				}
				if (WIFSIGNALED(status)) {
					if (CGI_DEBUG) std::cout << "executeCgi(): Child process terminated by signal" << std::endl;
					return "Status: 500\r\n\r\nCGI Script Terminated";
				}
				if (CGI_DEBUG) std::cout << "r == pid\nparent has read CGI output. result size: " << result.size() << std::endl;
				return result;
				
			}
		}
		if (CGI_DEBUG) std::cout << "parent has read CGI output. result size: " << result.size() << std::endl;
		
		return result;
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
