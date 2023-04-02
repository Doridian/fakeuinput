#include "server.h"
#include "clients.h"

#include "util.h"
#include "../shared/base.h"

#include <pthread.h>

void fakedevmgr_server_close(struct libevdev_uinput* evdev) {
    const int sockfd = evdev->sockfd;
    if (sockfd) {
        evdev->sockfd = 0;
        close(sockfd);
        unlink(evdev->devnode);
    }

    const pthread_t ptid_accept = evdev->ptid_accept;
    if (ptid_accept) {
        pthread_join(ptid_accept, NULL);
        evdev->ptid_accept = 0;
    }
}

void fakedevmgr_server_broadcast(const struct libevdev_uinput* evdev, struct input_event* ie, struct libevdev_client* except) {
    for (int i = 0; i < MAX_CLIENT_FDS; i++) {
        struct libevdev_client* client = evdev->clients[i];
        if (client != except && client && client->sockfd) {
            if (write(client->sockfd, ie, sizeof(struct input_event)) != sizeof(struct input_event)) {
                errlogf("write(broadcast) failed: %s", strerror(errno));
                fakedevmgr_client_close(client);
            }
        }
    }
}

static void* fakedevmgr_server_thread(void* arg) {
    struct libevdev_uinput* evdev = (struct libevdev_uinput*)arg;

    struct sockaddr_un client_addr;
    unsigned int client_addr_len;

    while (1) {
        const int sockfd = evdev->sockfd;
        if (sockfd == 0) {
            break;
        }
        client_addr_len = sizeof(client_addr);
        int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (clientfd < 0) {
            if (errno == EAGAIN) {
                continue;
            }
            errlogf("accept(%d) failed: %s", sockfd, strerror(errno));
            continue;
        }

        int foundindex = -1;
        for (int i = 0; i < MAX_CLIENT_FDS; i++) {
            if (fakedevmgr_cleanup_client_if_gone(evdev, i)) {
                continue;
            }

            if (set_socket_timeouts(clientfd)) {
                foundindex = -2;
                break;
            }

            if (write(clientfd, &evdev->fakedev, sizeof(struct fakedev_t)) != sizeof(struct fakedev_t)) {
                errlogf("write(welcome) failed: %s", strerror(errno));
                close(clientfd);
                foundindex = -2;
                break;
            }

            struct libevdev_client* client = malloc(sizeof(struct libevdev_client));
            client->sockfd = clientfd;
            client->index = i;
            client->evdev = evdev;
            evdev->clients[i] = client;
            pthread_create(&client->ptid_poll, NULL, (void*)fakedevmgr_client_thread, client);
            foundindex = i;
            break;
        }

        if (foundindex == -1) {
            errlog("Too many clients");
            close(clientfd);
            continue;
        }
    }

    for (int i = 0; i < MAX_CLIENT_FDS; i++) {
        fakedevmgr_client_close(evdev->clients[i]);
    }
    for (int i = 0; i < MAX_CLIENT_FDS; i++) {
        fakedevmgr_cleanup_client_if_gone(evdev, i);
    }

    pthread_exit(NULL);
    return NULL;
}

int fakedevmgr_server_start(struct libevdev_uinput* evdev) {
    unlink(evdev->devnode);

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        errlogf("socket() failed: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, evdev->devnode);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        errlogf("bind() failed: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 1) < 0) {
        errlogf("listen() failed: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    if (set_socket_timeouts(sockfd)) {
        return -1;
    }

    evdev->sockfd = sockfd;

    pthread_create(&evdev->ptid_accept, NULL, (void*)fakedevmgr_server_thread, evdev);

    return 0;
}
