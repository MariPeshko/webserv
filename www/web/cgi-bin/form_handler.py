import os
import sys
import urllib.parse
import cgitb

# Enable debug output
cgitb.enable()

print("Content-Type: text/html")
print("")

print("<html>")
print("<head><title>Form Result</title></head>")
print("<body>")
print("<h1>Form Submission Received From Python CGI</h1>")

# Manually parse the form data to avoid FieldStorage issues
try:
    # Read content length
    content_length = int(os.environ.get('CONTENT_LENGTH', 0))
    
    # Read the body directly from binary stdin
    body = sys.stdin.buffer.read(content_length).decode('utf-8')
    
    # Parse the query string format (name=val&msg=val)
    form_data = urllib.parse.parse_qs(body)
    
    # Helper to get value
    def get_val(key):
        if key in form_data:
            return form_data[key][0] # parse_qs returns a list for each key
        return None

    name = get_val("name")
    message = get_val("message")

    if name and message:
        print(f"<p><b>Name:</b> {name}</p>")
        print(f"<p><b>Message:</b> {message}</p>")
    else:
        print("<p><i>Error: Missing name or message fields.</i></p>")

except Exception as e:
    print(f"<p><i>Error parsing form data: {e}</i></p>")

print("<h2>Debug Info</h2>")
print(f"<p>Method: {os.environ.get('REQUEST_METHOD', 'Unknown')}</p>")
print("</body>")
print("</html>")
