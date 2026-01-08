#!/bin/bash

echo "Starting cleanup test..."

# Start server in background
./webserv &
SERVER_PID=$!
sleep 2

echo "Server PID: $SERVER_PID"
echo "Open file descriptors before signal:"
lsof -p $SERVER_PID

echo "Sending SIGTERM..."
kill -TERM $SERVER_PID

# Wait for graceful shutdown
wait $SERVER_PID
EXIT_CODE=$?

echo "Server exit code: $EXIT_CODE"
echo "Checking for remaining processes..."
pgrep webserv || echo "No webserv processes found ✅"

echo "Checking port 8080..."
lsof -i :8080 || echo "Port 8080 is free ✅"

echo "Cleanup test completed!"