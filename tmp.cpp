// We got some good data from a client
    Client& client = _clients[sender_fd];
    client.request_buffer.append(buf, nbytes);

    // --- PARSER STATE MACHINE ---
    // This is where you will implement the logic.
    // It should be a loop, in case a single read contains the whole request.
    bool request_is_ready = false;
    while (!request_is_ready) {
        switch (client.state) {
            case READING_REQUEST_LINE:
                // Search for "\r\n" in client.request_buffer
                // If found:
                //   - Parse the line (method, URI, version)
                //   - Remove the line from the buffer
                //   - Change state: client.state = READING_HEADERS;
                // Else (not found):
                //   - break from the while loop and wait for more data
                break;

            case READING_HEADERS:
                // Search for "\r\n\r\n" in client.request_buffer
                // If found:
                //   - Parse all headers
                //   - Check for Content-Length or Transfer-Encoding
                //   - Remove headers from the buffer
                //   - If body is expected: client.state = READING_BODY;
                //   - Else: client.state = REQUEST_COMPLETE; request_is_ready = true;
                // Else (not found):
                //   - break from the while loop
                break;

            case READING_BODY:
                // If using Content-Length:
                //   - Check if client.request_buffer.size() >= content_length
                //   - If yes: request is complete. client.state = REQUEST_COMPLETE; request_is_ready = true;
                // If using chunked encoding:
                //   - Parse chunks until the final "0\r\n\r\n" is found.
                //   - If final chunk found: client.state = REQUEST_COMPLETE; request_is_ready = true;
                // Else (body not fully received):
                //   - break from the while loop
                break;
            
            case REQUEST_COMPLETE:
                request_is_ready = true;
                break;

            case REQUEST_ERROR:
                // Handle error, maybe set a flag to send 400 Bad Request
                request_is_ready = true;
                break;
        }
        if (client.state == READING_REQUEST_LINE || client.state == READING_HEADERS || client.state == READING_BODY) {
             // If we are still reading, break the loop to wait for more data
             break;
        }
    }

    if (request_is_ready) {
        std::cout << "Request from " << sender_fd << " is fully received and parsed." << std::endl;
        // TODO: Process the request and send a response.
        // After sending response, you might close the socket or reset the client state
        // for keep-alive connections.
        // For now, let's just close it.
        close(sender_fd);
        _clients.erase(sender_fd);
        delFromPfds(i);
        if (i > 0) --i;
    }
}