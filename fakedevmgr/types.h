#pragma once

#include "../shared/types.h"

#define MAX_CLIENT_FDS 256

#include <pthread.h>

struct libevdev {
    struct fakedev_t fakedev;
};

struct libevdev_uinput {
    int sockfd;

    int local_clientfd;

    pthread_t ptid_accept;
    char devnode[256];

    struct libevdev_client* clients[MAX_CLIENT_FDS];

    struct fakedev_t fakedev;
};

struct libevdev_client {
    int sockfd;

    int index;
    pthread_t ptid_poll;
    struct libevdev_uinput* evdev;
};
