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
    struct fakedevmgr_client_list_item* next = evdev->first_client_item;
    while (next) {
        struct libevdev_client* client = next->client;
        next = next->next;
        if (!client || client == except || !client->sockfd) {
            continue;
        }
        if (write(client->sockfd, ie, sizeof(struct input_event)) != sizeof(struct input_event)) {
            errlogf("write(broadcast) failed: %s", strerror(errno));
            fakedevmgr_client_close(client);
        }
    }
}

static void fakedevmgr_server_remove_client(struct libevdev_uinput* evdev, struct fakedevmgr_client_list_item* item) {
    if (!item) {
        return;
    }

    if (item->prev) {
        item->prev->next = item->next;
    }
    if (item->next) {
        item->next->prev = item->prev;
    }
    if (evdev->first_client_item == item) {
        evdev->first_client_item = item->next;
    }

    fakedevmgr_client_close_wait(item->client);
    free(item->client);
    free(item);
}

static void fakedevmgr_server_cleanup_clients(struct libevdev_uinput* evdev) {
    struct fakedevmgr_client_list_item* next = evdev->first_client_item;
    while (next) {
        struct fakedevmgr_client_list_item* item = next;
        next = item->next;

        if (fakedevmgr_cleanup_client_if_gone(item->client)) {
            continue;
        }
        fakedevmgr_server_remove_client(evdev, item);
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
        
        fakedevmgr_server_cleanup_clients(evdev);
        
        client_addr_len = sizeof(client_addr);
        int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (clientfd < 0) {
            if (errno == EAGAIN) {
                continue;
            }
            errlogf("accept(%d) failed: %s", sockfd, strerror(errno));
            continue;
        }

        if (set_socket_timeouts(clientfd)) {
            close(clientfd);
            continue;
        }

        if (write(clientfd, &evdev->fakedev, sizeof(struct fakedev_t)) != sizeof(struct fakedev_t)) {
            errlogf("write(welcome) failed: %s", strerror(errno));
            close(clientfd);
            continue;
        }

        struct libevdev_client* client = malloc(sizeof(struct libevdev_client));
        client->sockfd = clientfd;
        client->evdev = evdev;

        struct fakedevmgr_client_list_item* item = malloc(sizeof(struct fakedevmgr_client_list_item));
        item->client = client;

        struct fakedevmgr_client_list_item* next = evdev->first_client_item;
        struct fakedevmgr_client_list_item* last_item = NULL;
        while (next) {
            last_item = next;
            next = next->next;
        }

        if (last_item) {
            last_item->next = item;
            item->next = NULL;
            item->prev = last_item;
        } else {
            evdev->first_client_item = item;
            item->next = NULL;
            item->prev = NULL;
        }

        pthread_create(&client->ptid_poll, NULL, (void*)fakedevmgr_client_thread, client);
    }

    struct fakedevmgr_client_list_item* next = evdev->first_client_item;
    while (next) {
        fakedevmgr_client_close(next->client);
        next = next->next;
    }
    next = evdev->first_client_item;
    while (next) {
        struct fakedevmgr_client_list_item* item = next;
        next = item->next;

        fakedevmgr_cleanup_client_if_gone(item->client);
        free(item->client);
        free(item);
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
