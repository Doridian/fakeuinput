#pragma once

#include "../shared/types.h"

#include <pthread.h>

struct libevdev { // Config
    struct fakedev_t fakedev;
};

struct fakedevmgr_client_list_item {
    struct fakedevmgr_client* client;
    struct fakedevmgr_client_list_item* next;
    struct fakedevmgr_client_list_item* prev;
};

struct libevdev_uinput { // Server
    int sockfd;

    int local_clientfd;

    pthread_t ptid_accept;
    char devnode[256];

    struct fakedevmgr_client_list_item* first_client_item;

    struct fakedev_t fakedev;
};

struct fakedevmgr_client { // Client
    int sockfd;

    pthread_t ptid_poll;
    struct libevdev_uinput* evdev;
};
