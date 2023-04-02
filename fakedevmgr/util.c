#include "util.h"

#include <sys/socket.h>
#include <errno.h>

int set_socket_timeouts(int sockfd) {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) < 0) {
        errlogf("setsockopt(SO_SNDTIMEO) failed: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
        errlogf("setsockopt(SO_RCVTIMEO) failed: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    return 0;
}
