// beej
#include <stdio.h>
#include <poll.h>


/***
 * int poll(struct pollfd fds[], nfds_t nfds, int timeout);
 * @param fds is our array of information (which sockets to monitor
 * for what). both the listening server sockets and 
 * all the connected client sockets
 * @param nfds is the count of elements in the array
 * @param timeout is a timeout in milliseconds.
 * 
 * It returns the number of elements in the array that have 
 * had an event occur.
 */

/*  struct pollfd {
    int fd;         // the socket descriptor
    short events;   // bitmap of events we're interested in
    short revents;  // on return, bitmap of events that occurred
}; */

int main(void)
{
    struct pollfd pfds[1]; // More if you want to monitor more

    pfds[0].fd = 0;          // Standard input
    pfds[0].events = POLLIN; // Tell me when ready to read

    // If you needed to monitor other things, as well:
    //pfds[1].fd = some_socket; // Some socket descriptor
    //pfds[1].events = POLLIN;  // Tell me when ready to read

    printf("Hit RETURN or wait 2.5 seconds for timeout\n");

    int num_events = poll(pfds, 1, 2500); // 2.5 second timeout

    if (num_events == 0) {
        printf("Poll timed out!\n");
    } else {
        int pollin_happened = pfds[0].revents & POLLIN;

        if (pollin_happened) {
            printf("File descriptor %d is ready to read\n",
                    pfds[0].fd);
        } else {
            printf("Unexpected event occurred: %d\n",
                    pfds[0].revents);
        }
    }

    return 0;
}
