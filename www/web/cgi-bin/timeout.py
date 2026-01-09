#!/usr/bin/env python3

# a simple Python CGI script that will exceed your 5s timeout by 
# sleeping 10 seconds. It writes a valid CGI header but delays the 
# body, causing your server to kill it and return 504.

import sys
import time

# Emit minimal CGI headers
sys.stdout.write("Status: 200 OK\r\n")
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.flush()

# Sleep longer than your CGI timeout (5s)
time.sleep(10)

# This body will likely never be seen because the parent will timeout
sys.stdout.write("This should not appear; CGI exceeded timeout.\n")
sys.stdout.flush()