#pragma once

#include "types.h"

void fakedevmgr_client_close(struct libevdev_client* client);
void fakedevmgr_client_close_wait(struct libevdev_client* client);
int fakedevmgr_cleanup_client_if_gone(struct libevdev_client* client);
void* fakedevmgr_client_thread(void* arg);
