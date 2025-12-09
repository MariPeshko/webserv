import http.client
import sys
import time

# Configuration
HOST = "127.0.0.1"
PORT = 8080

# Colors
GREEN = "\033[92m"
RED = "\033[91m"
RESET = "\033[0m"

def run_test(method, path, expected_status, description, check_header=None, port=PORT):
    print(f"Testing {description} [{method} {path}]...", end=" ")
    try:
        conn = http.client.HTTPConnection(HOST, port)
        conn.request(method, path)
        response = conn.getresponse()
        data = response.read() # Read body to clear channel
        conn.close()
        
        if response.status == expected_status:
            if check_header:
                header_val = response.getheader(check_header[0])
                if header_val != check_header[1]:
                     print(f"{RED}FAIL{RESET} (Status OK, but header {check_header[0]} expected '{check_header[1]}' got '{header_val}')")
                     return False

            print(f"{GREEN}PASS{RESET}")
            return True
        else:
            print(f"{RED}FAIL{RESET} (Expected {expected_status}, Got {response.status})")
            return False
    except Exception as e:
        print(f"{RED}ERROR{RESET} (Connection failed: {e})")
        return False

def main():
    print(f"Running tests against {HOST}:{PORT}...\n")
    
    tests = [
        # Basic Static Files
        ("GET", "/", 200, "Root Index Page"),
        ("GET", "/index.html", 200, "Explicit Index File"),
        ("GET", "/about.html", 200, "About Page"),
        ("GET", "/favicon.ico", 200, "Favicon"),
        
        # Directory Handling
        ("GET", "/about", 200, "Directory with Index (should serve about.html)"),
        
        # Error Handling
        ("GET", "/this-does-not-exist", 404, "Non-existent Page (404)"),
        
        # Method Restrictions
        ("DELETE", "/about", 405, "Method Not Allowed (DELETE on /about)"),
        ("POST", "/gallery", 405, "Method Not Allowed (POST on /gallery)"),
        
        # Redirection
        ("GET", "/old-page", 301, "Redirect /old-page -> /new-page"),
        ("GET", "/new-page", 200, "New Page (Redirect Target)"),
        
        # Autoindex
        ("GET", "/correct-auto-index", 200, "Autoindex Directory Listing"),
        ("GET", "/wrong-auto-index", 403, "Directory Forbidden (Autoindex Off, No Index)"),
        ("GET", "/auto-index-off", 403, "Directory Forbidden (Autoindex Off)"),
        
        # Gallery
        ("GET", "/gallery", 200, "Gallery Page (Index)"),

        # Second Server (Port 8081)
        ("GET", "/", 301, "Server 2: Redirect Root -> /additional", None, 8081),
        ("GET", "/additional", 200, "Server 2: Additional Page", None, 8081),
    ]
    
    passed = 0
    for test in tests:
        # Handle optional header check and port
        header_check = None
        port = PORT
        
        if len(test) > 4:
            header_check = test[4]
        if len(test) > 5:
            port = test[5]
            
        if run_test(test[0], test[1], test[2], test[3], header_check, port):
            passed += 1
            
    print(f"\nSummary: {passed}/{len(tests)} tests passed.")
    
    if passed != len(tests):
        sys.exit(1)

if __name__ == "__main__":
    # Wait a bit for server to start if called from script
    time.sleep(1)
    main()
