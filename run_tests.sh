#!/bin/bash

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building webserv...${NC}"
make re

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo -e "${GREEN}Starting webserv in background...${NC}"
./webserv configs/default.conf > /dev/null 2>&1 &
SERVER_PID=$!

echo "Server PID: $SERVER_PID"
echo "Waiting for server to initialize..."
sleep 2

echo -e "${GREEN}Running Python tests...${NC}"
python3 tests/test_endpoints.py

TEST_EXIT_CODE=$?

echo -e "${GREEN}Stopping server...${NC}"
kill $SERVER_PID

if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed.${NC}"
    exit 1
fi
