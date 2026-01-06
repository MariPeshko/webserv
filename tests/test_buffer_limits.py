#!/usr/bin/env python3
"""
Test cases for HTTP 414 (URI Too Long) and 431 (Request Header Fields Too Large)
These tests verify that the server properly rejects requests with excessive sizes.
"""

import socket
import sys

def test_414_uri_too_long(host='localhost', port=8080):
    """
    Test Case 1: 414 URI Too Long
    
    Sends a GET request with a URI exceeding 2048 bytes (MAX_REQUEST_LINE_SIZE).
    Expected: Server should respond with 414 URI Too Long.
    """
    print("\n" + "="*70)
    print("TEST 1: 414 URI Too Long")
    print("="*70)
    
    # Create a URI that exceeds 2048 bytes
    # Request line format: "GET /path HTTP/1.1\r\n" 
    # So the path needs to be large enough to make the entire line > 2048 bytes
    base_request = "GET /"
    http_version = " HTTP/1.1\r\n"
    overhead = len(base_request) + len(http_version)
    
    # Create path that makes total request line > 2048 bytes
    path_size = 2060 - overhead  # Slightly over the limit
    long_path = "a" * path_size
    
    request = base_request + long_path + http_version + "\r\n"
    
    print(f"Request line size: {len(base_request + long_path + http_version) - 2} bytes")
    print(f"Expected: 414 URI Too Long")
    print(f"Connecting to {host}:{port}...")
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect((host, port))
        
        # Send the request
        sock.sendall(request.encode())
        
        # Receive response
        response = sock.recv(4096).decode('utf-8', errors='ignore')
        sock.close()
        
        print("\nResponse received:")
        print("-" * 70)
        # Print first 17 chars of response
        print(response[:17])
        print("-" * 70)
        
        # Check if we got 414
        if "414" in response and "URI Too Long" in response:
            print("\n✓ TEST PASSED: Server returned 414 URI Too Long")
            return True
        else:
            print("\n✗ TEST FAILED: Expected 414 but got:")
            print(response.split('\n')[0])
            return False
            
    except socket.timeout:
        print("\n✗ TEST FAILED: Connection timeout")
        return False
    except Exception as e:
        print(f"\n✗ TEST FAILED: {e}")
        return False


def test_431_headers_too_large(host='localhost', port=8080):
    """
    Test Case 2: 431 Request Header Fields Too Large
    
    Sends a GET request with headers exceeding 16384 bytes (MAX_HEADER_BLOCK_SIZE).
    Note: This check only happens after successful request line parsing.
    Expected: Server should respond with 431 Request Header Fields Too Large.
    """
    print("\n" + "="*70)
    print("TEST 2: 431 Request Header Fields Too Large")
    print("="*70)
    
    # Create a valid, short request line (must be under 2048 bytes to pass first check)
    request = "GET / HTTP/1.1\r\n"
    request += "Host: localhost\r\n"
    
    # Track header size (excluding request line which was already parsed)
    # After request line is parsed, only headers remain in buffer
    initial_size = len(request)
    
    # Add many large headers to exceed 16384 bytes
    # Each custom header: "X-Custom-Header-NNN: <value>\r\n"
    header_count = 0
    current_size = len(request)
    
    # Keep adding headers until we exceed the limit
    # Target: slightly over 16384 bytes for the header portion
    while current_size < initial_size + 16400:  # Slightly over 16384 limit
        header_name = f"X-Custom-Header-{header_count}"
        # Each header value is 1000 bytes
        header_value = "a" * 1000
        header_line = f"{header_name}: {header_value}\r\n"
        request += header_line
        current_size += len(header_line)
        header_count += 1
    
    request += "\r\n"  # End of headers
    
    # Calculate header block size (after request line is removed)
    header_block_size = current_size - initial_size
    print(f"Request line size: {initial_size - 2} bytes (valid)")
    print(f"Header block size (after request line parsing): {header_block_size} bytes")
    print(f"Number of custom headers added: {header_count}")
    print(f"Expected: 431 Request Header Fields Too Large")
    print(f"Connecting to {host}:{port}...")
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect((host, port))
        
        # Send the request
        sock.sendall(request.encode())
        
        # Receive response
        response = sock.recv(4096).decode('utf-8', errors='ignore')
        sock.close()
        
        print("\nResponse received:")
        print("-" * 70)
        # Print first 46 chars of response
        print(response[:46])
        print("-" * 70)
        
        # Check if we got 431
        if "431" in response and ("Request Header Fields Too Large" in response or "Request Header" in response):
            print("\n✓ TEST PASSED: Server returned 431 Request Header Fields Too Large")
            return True
        else:
            print("\n✗ TEST FAILED: Expected 431 but got:")
            print(response.split('\n')[0])
            return False
            
    except socket.timeout:
        print("\n✗ TEST FAILED: Connection timeout")
        return False
    except Exception as e:
        print(f"\n✗ TEST FAILED: {e}")
        return False


def main():
    """Run all tests"""
    if len(sys.argv) > 1:
        host = sys.argv[1]
    else:
        host = 'localhost'
    
    if len(sys.argv) > 2:
        port = int(sys.argv[2])
    else:
        port = 8080
    
    print("="*70)
    print(f"Testing HTTP Server at {host}:{port}")
    print("Testing Buffer Growth Control (414 and 431 status codes)")
    print("="*70)
    
    results = []
    
    # Run tests
    results.append(("414 URI Too Long", test_414_uri_too_long(host, port)))
    results.append(("431 Headers Too Large", test_431_headers_too_large(host, port)))
    
    # Summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✓ PASSED" if result else "✗ FAILED"
        print(f"{test_name}: {status}")
    
    print(f"\nTotal: {passed}/{total} tests passed")
    print("="*70)
    
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
