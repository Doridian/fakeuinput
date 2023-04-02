#include "clients.h"
#include "server.h"

#include "util.h"
#include "../shared/base.h"

#include <pthread.h>

void fakedevmgr_client_close(struct libevdev_client* client) {
    if (!client) {
        return;
    }

    const int sockfd = client->sockfd;
    if (sockfd) {
        client->sockfd = 0;
        close(sockfd);
    }
}

void fakedevmgr_client_close_wait(struct libevdev_client* client) {
    fakedevmgr_client_close(client);
    const pthread_t ptid_poll = client->ptid_poll;
    if (ptid_poll) {
        pthread_join(ptid_poll, NULL);
        client->ptid_poll = 0;
    }
}

int fakedevmgr_cleanup_client_if_gone(struct libevdev_client* client) {
    if (!client) {
        return 0;
    }

    if (client->sockfd) {
        return 1;
    }

    fakedevmgr_client_close_wait(client);
    return 0;
}

void* fakedevmgr_client_thread(void* arg) {
    struct libevdev_client* client = (struct libevdev_client*)arg;
    struct input_event ie;

    while (1) {
        const int sockfd = client->sockfd;
        if (sockfd == 0) {
            break;
        }
        if (read(sockfd, &ie, sizeof(struct input_event)) != sizeof(struct input_event)) {
            if (errno == EAGAIN) {
                continue;
            }
            errlogf("read(client %d, %p) failed: %s", sockfd, client, strerror(errno));
            fakedevmgr_client_close(client);
            break;
        }
        fakedevmgr_server_broadcast(client->evdev, &ie, client);
    }

    client->ptid_poll = 0;
    pthread_exit(NULL);
    return NULL;
}
