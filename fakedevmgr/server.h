#pragma once

#include "types.h"

void fakedevmgr_server_close(struct libevdev_uinput* evdev);
void fakedevmgr_server_broadcast(const struct libevdev_uinput* evdev, struct input_event* ie, struct libevdev_client* except);
int fakedevmgr_server_start(struct libevdev_uinput* evdev);
