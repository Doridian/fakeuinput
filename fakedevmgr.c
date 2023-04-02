#include "base.h"

#include <fcntl.h>

struct fakedev_t FAKEDEV_BASE = {
    .initialized = false,
    .id = {
        .bustype = BUS_VIRTUAL,
        .vendor = 0x43,
        .product = 0x69,
        .version = 0x1,
    },
    .name = "fakedev",
};

int main(int argc, const char** argv) {
    const char* devname = argv[1];
    if (!strstr(devname, FAKEDEV_PATH)) {
        return 1;
    }

    unlink(devname);

    struct fakedev_t dev;
    memcpy(&dev, &FAKEDEV_BASE, sizeof(struct fakedev_t));
    set_bit(EV_SYN, dev.evbit);
    set_bit(EV_ABS, dev.evbit);
    set_bit(EV_KEY, dev.evbit);

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("socket() failed: %s\n", strerror(errno));
        return 10;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, devname);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("bind() failed: %s\n", strerror(errno));
        close(sockfd);
        return 11;
    }

    if (listen(sockfd, 1) < 0) {
        printf("listen() failed: %s\n", strerror(errno));
        close(sockfd);
        return 12;
    }

    while (1) {
        int clientfd = accept(sockfd, NULL, NULL);
        if (clientfd < 0) {
            printf("accept() failed: %s\n", strerror(errno));
            continue;
        }
        write(clientfd, &dev, sizeof(struct fakedev_t));
    }

    return 0;
}
