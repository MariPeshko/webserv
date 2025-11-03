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
