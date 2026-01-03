# webserv
This project aims to create our own HTTP server.

## FEATURES

- Supports HTTP/1.0 and HTTP/1.1 request parsing (basic methods: GET/POST/DELETE).
- Persistent connections (keep-alive) in sequential mode (one active request at a time).
- Chunked transfer encoding (incoming request bodies) supported.
- Static file serving.
- Basic CGI execution based on file extension (e.g. `.php`, `.py`, etc.).
- Graceful handling of SIGPIPE via MSG_NOSIGNAL on send().

## LIMITATIONS (LEARNING PURPOSE)

This server intentionally keeps the HTTP model minimal:

- No HTTP/1.1 request pipelining: only one in‑flight request per connection. A new request is expected after the previous response is fully sent.
- No multiplexing/protocol upgrade (no HTTP/2, no WebSocket).
- Keep-Alive is tolerated but sequential: request → response → reset → wait.
- CGI selection is purely by file extension (no shebang resolution, no FCGI).
- Limited header validation; unsupported/complex features (Expect: 100-continue, Range, etc.) are ignored.
- Routing / virtual host logic is minimal and may not reflect full Nginx‑style semantics.

If you use specialized tools (e.g. wrk, curl with --next, custom scripts) that rely on pipelining, results will not reflect a fully compliant HTTP/1.1 server.

## TESTING REMINDER

Use telnet or nc to send a single full request, then wait for the response before sending the next one.

Example:
```
printf "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080
```

### Telnet quick test

```
telnet localhost 8080
GET /index.php HTTP/1.1
Host: localhost

```

### To test it easily, you need two terminals - one running the server, one acting as the client.

Using telnet. 
1. Open a new terminal window. Type:

> telnet localhost 8080

2. You should see something like:

```
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
```

3. In the telnet terminal, type some text and press Enter. The server will echo it back.

4. To exit telnet, press `Ctrl+]` then type `quit` and press Enter.


## SIGNALS

**Basic**

You can find information about Unix signals in terminal with a command "man 7 signal"

Signal       Keyboard Shortcut

> SIGINT       Ctrl + C
Terminates the process gracefully, but can be caught or ignored by the program for cleanup.

> SIGTERM      None (sent via kill command)
Requests the process to terminate gracefully. It is the default signal sent by the kill command (kill [pid]), allowing the program to perform clean-up operations before exiting.

> SIGQUIT      Ctrl + \\
Terminates the process and, by default, produces a core dump

**Properly terminate the suspended process:**

Check for suspended/running processes on port 8080
> ps aux | grep webserv

Or check what's using port 8080
> lsof -i :8080

Kill the suspended process
> kill %1    # Kill job 1 (the suspended process)
or
> kill 4169  # Kill by PID

## DEBUGGING SOCKETS

**Check if all client connections are properly closed:**

1. **Using lsof (simplest method):**
```bash
# Check all open files/sockets for your process
lsof -p [PID_of_your_process]

# Or check specific port
lsof -i :8080

# Show only sockets
lsof -i -a -p [PID]
```

2. **Using netstat (shows network connections and listening ports):**
```bash
# Check all connections on port 8080
netstat -anp | grep 8080

# -a: Show all connections (listening and established)
# -n: Show numerical addresses (don't resolve hostnames)  
# -p: Show process ID and name that owns each socket
```

3. **Using /proc filesystem:**
```bash
# View all open file descriptors
ls -la /proc/[PID]/fd/

# For running webserv process
ps aux | grep webserv    # find PID
ls -la /proc/[PID]/fd/   # view open fd
```

4. **Using strace to track system calls:**
```bash
# Track all socket operations including close()
strace -e trace=socket,bind,listen,accept,close ./webserv
```

## SIGPIPE & MSG_NOSIGNAL flag for send()

How to trigger SIGPIPE

```sleep(5);                           // Give time for client to potentially disconnect
shutdown(client_fd, SHUT_WR);       // Shutdown writing side of socket
send(client_fd, "Second message\n", 15, 0);  // Try to send to shutdown socket → SIGPIPE!```

With MSG_NOSIGNAL - No signal generated, just returns -1 with errno


## /.well-known/appspecific/com.chrome.devtools.json
The request for /.well-known/appspecific/com.chrome.devtools.json is a standard behavior of Google Chrome's Developer Tools.

When you have Chrome DevTools open, it automatically probes the web server for a special configuration file. This file is intended to help with source mapping, which allows the debugger to show you your original source code (e.g., TypeScript, unminified JavaScript) instead of the code that's actually running in the browser.

Your server receives the request, looks for the file /www/web/.well-known/appspecific/com.chrome.devtools.json, and doesn't find it. It correctly responds with a 404 Not Found error.