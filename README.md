# webserv
This project aims to create our own HTTP server.


### To compile with C++98:
g++ -Wall -Wextra -Werror -std=c++98 -pedantic main.cpp -o webserv


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

2. **Using /proc filesystem:**
```bash
# View all open file descriptors
ls -la /proc/[PID]/fd/

# For running webserv process
ps aux | grep webserv    # find PID
ls -la /proc/[PID]/fd/   # view open fd
```

3. **Using strace to track system calls:**
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