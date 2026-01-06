import os
import sys

# Print Headers
print("Content-Type: text/html")
print("") # Empty line marks end of headers

# Print Body
print("<html>")
print("<head><title>CGI Test</title></head>")
print("<body>")
print("<h1>Hello from Python CGI!</h1>")
print("<h2>Environment Variables:</h2>")
print("<ul>")
for param in os.environ.keys():
    print(f"<li><b>{param}</b>: {os.environ[param]}</li>")
print("</ul>")
print("</body>")
print("</html>")