#pragma once

#include "types.h"

void fakedevmgr_client_close(struct libevdev_client* client);
int fakedevmgr_cleanup_client_if_gone(struct libevdev_uinput* evdev, int index);
void* fakedevmgr_client_thread(void* arg);
