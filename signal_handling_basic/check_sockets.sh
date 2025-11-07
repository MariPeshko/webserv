#!/bin/bash

# Terminal 2: Check sockets while running

echo "Checking sockets for webserv process..."
PID=$(pgrep webserv)
if [ -n "$PID" ]; then
    echo "Process PID: $PID"
    echo "Open file descriptors: $(ls /proc/$PID/fd/ | wc -l)"
    echo "Socket connections:"
    lsof -p $PID | grep socket
else
    echo "No webserv process found"
fi