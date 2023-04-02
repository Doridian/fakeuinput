#include "../shared/base.h"
#include "types.h"
#include "util.h"
#include "server.h"

#include <stdlib.h>
#include <stdio.h>

int libevdev_uinput_create_from_device(const struct libevdev* dev, int uinput_fd, struct libevdev_uinput** uinput_dev) {
    struct libevdev_uinput* uinput_dev_tmp = calloc(1, sizeof(struct libevdev_uinput));

    for (int i = 0; i < 255; i++) {
        snprintf(uinput_dev_tmp->devnode, sizeof(uinput_dev_tmp->devnode), FAKEDEV_PATH "event%d", i);
        if (access(uinput_dev_tmp->devnode, F_OK) != 0) {
            break;
        }
        uinput_dev_tmp->devnode[0] = 0;
    }

    if (uinput_dev_tmp->devnode[0] == 0) {
        free(uinput_dev_tmp);
        errlog("libevdev_uinput_create_from_device(): no free event device");
        return -1;
    }

    memcpy(&uinput_dev_tmp->fakedev, &dev->fakedev, sizeof(struct fakedev_t));

    if (fakedevmgr_server_start(uinput_dev_tmp) < 0) {
        free(uinput_dev_tmp);
        errlog("fakedevmgr_server_start() failed");
        return -1;
    }

    struct sockaddr_un un;
    const int fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (unlikely(fd < 0)) {
        errlogf("socket(%s) failed: %s", uinput_dev_tmp->devnode, strerror(errno));
        return -EINVAL;
    }

    un.sun_family = AF_LOCAL;
    strncpy(un.sun_path, uinput_dev_tmp->devnode, sizeof(un.sun_path));

    if (unlikely(connect(fd, (struct sockaddr *)&un, sizeof(un)) < 0)) {
        errlogf("connect(%d, %s) failed: %s", fd, uinput_dev_tmp->devnode, strerror(errno));
        close(fd);
        return -EINVAL;
    }

    struct fakedev_t fakedev;
    read(fd, &fakedev, sizeof(struct fakedev_t));
    uinput_dev_tmp->local_clientfd = fd;

    *uinput_dev = uinput_dev_tmp;
    return 0;
}

const char* libevdev_uinput_get_devnode(struct libevdev_uinput* uinput_dev) {
    return uinput_dev->devnode;
}

int libevdev_uinput_get_fd(const struct libevdev_uinput* uinput_dev) {
    return uinput_dev->local_clientfd;
}

int libevdev_uinput_write_event(const struct libevdev_uinput* uinput_dev, unsigned int type, unsigned int code, int value) {
    struct input_event ie;
    ie.type = type;
    ie.code = code;
    ie.value = value;
    gettimeofday(&ie.time, NULL);

    fakedevmgr_server_broadcast(uinput_dev, &ie, NULL);
    return 0;
}

void libevdev_uinput_destroy(struct libevdev_uinput* uinput_dev) {
    fakedevmgr_server_close(uinput_dev);
    free(uinput_dev);
}
