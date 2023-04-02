#include "../shared/base.h"

#include <fcntl.h>
#include <pthread.h>

#define errlog(str) { fprintf(stderr, "fakedevmgr: " str "\n"); fflush(stderr); }
#define errlogf(str, ...) { fprintf(stderr, "fakedevmgr: " str "\n", __VA_ARGS__); fflush(stderr); }

#define MAX_CLIENT_FDS 256

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

static int set_socket_timeouts(int sockfd) {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) < 0) {
        errlogf("setsockopt(SO_SNDTIMEO) failed: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
        errlogf("setsockopt(SO_RCVTIMEO) failed: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    return 0;
}

static void client_close(struct libevdev_client* client) {
    if (!client) {
        return;
    }

    const int sockfd = client->sockfd;
    if (sockfd) {
        client->sockfd = 0;
        close(sockfd);
    }
}

static int cleanup_client_if_gone(struct libevdev_uinput* evdev, int index) {
    struct libevdev_client* client = evdev->clients[index];
    if (!client) {
        return 0;
    }

    if (client->sockfd) {
        return 1;
    }

    const pthread_t ptid_poll = client->ptid_poll;
    if (ptid_poll) {
        client->ptid_poll = 0;
        pthread_join(ptid_poll, NULL);
    }

    evdev->clients[index] = NULL;
    free(client);

    return 0;
}

static void shutdown_devhandler(struct libevdev_uinput* evdev) {
    const int sockfd = evdev->sockfd;
    if (sockfd) {
        evdev->sockfd = 0;
        close(sockfd);
        unlink(evdev->devnode);
    }
    for (int i = 0; i < MAX_CLIENT_FDS; i++) {
        client_close(evdev->clients[i]);
    }
    for (int i = 0; i < MAX_CLIENT_FDS; i++) {
        cleanup_client_if_gone(evdev, i);
    }
}

static void server_close(struct libevdev_uinput* evdev) {
    shutdown_devhandler(evdev);

    const pthread_t ptid_accept = evdev->ptid_accept;
    if (ptid_accept) {
        evdev->ptid_accept = 0;
        pthread_join(ptid_accept, NULL);
    }
}

void devhandler_broadcast(const struct libevdev_uinput* evdev, struct input_event* ie, struct libevdev_client* except) {
    for (int i = 0; i < MAX_CLIENT_FDS; i++) {
        struct libevdev_client* client = evdev->clients[i];
        if (client != except && client && client->sockfd) {
            if (write(client->sockfd, ie, sizeof(struct input_event)) != sizeof(struct input_event)) {
                errlogf("write(broadcast) failed: %s", strerror(errno));
                client_close(client);
            }
        }
    }
}

void* devhandler_client(void* arg) {
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
            errlogf("read(client %d) failed: %s", sockfd, strerror(errno));
            client_close(client);
            break;
        }
        devhandler_broadcast(client->evdev, &ie, client);
    }

    client->ptid_poll = 0;
    pthread_exit(NULL);
    return NULL;
}

void* devhandler_accept(void* arg) {
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
            errlogf("accept() failed: %s", strerror(errno));
            continue;
        }

        int foundindex = -1;
        for (int i = 0; i < MAX_CLIENT_FDS; i++) {
            if (cleanup_client_if_gone(evdev, i)) {
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
            pthread_create(&client->ptid_poll, NULL, (void*)devhandler_client, client);
            foundindex = i;
            break;
        }

        if (foundindex == -1) {
            errlog("Too many clients");
            close(clientfd);
            continue;
        }
    }

    shutdown_devhandler(evdev);
    pthread_exit(NULL);
    return NULL;
}

int init_devhandler(struct libevdev_uinput* evdev) {
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

    pthread_create(&evdev->ptid_accept, NULL, (void*)devhandler_accept, evdev);

    return 0;
}

struct libevdev* libevdev_new() {
    return calloc(1, sizeof(struct libevdev));
}

void libevdev_free(struct libevdev* dev) {
    free(dev);
}

void libevdev_set_uniq(struct libevdev* dev, const char* uniq) {
    strncpy(dev->fakedev.uniq, uniq, sizeof(dev->fakedev.uniq));
}

void libevdev_set_name(struct libevdev* dev, const char* name) {
    strncpy(dev->fakedev.name, name, sizeof(dev->fakedev.name));
}

void libevdev_set_id_product(struct libevdev* dev, int product_id) {
    dev->fakedev.id.product = product_id;
}

void libevdev_set_id_vendor(struct libevdev* dev, int vendor_id) {
    dev->fakedev.id.vendor = vendor_id;
}

void libevdev_set_id_bustype(struct libevdev* dev, int bustype) {
    dev->fakedev.id.bustype = bustype;
}

void libevdev_set_id_version(struct libevdev* dev, int version) {
    dev->fakedev.id.version = version;
}

int libevdev_enable_event_type(struct libevdev* dev, unsigned int type) {
    set_bit(type, dev->fakedev.evbit);
    return 0;
}

int libevdev_enable_event_code(struct libevdev* dev, unsigned int type, unsigned int code, const void *data) {
    switch (type) {
        case EV_ABS:
            set_bit(code, dev->fakedev.absbit);
            return 0;
        case EV_KEY:
            set_bit(code, dev->fakedev.keybit);
            return 0;
    }
    return -1;
}

int libevdev_enable_property(struct libevdev* dev, unsigned int prop) {
    set_bit(prop, dev->fakedev.propbit);
    return 0;
}

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

    errlogf("libevdev_uinput_create_from_device(): using %s", uinput_dev_tmp->devnode);

    memcpy(&uinput_dev_tmp->fakedev, &dev->fakedev, sizeof(struct fakedev_t));

    if (init_devhandler(uinput_dev_tmp) < 0) {
        free(uinput_dev_tmp);
        errlog("init_devhandler() failed");
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

    devhandler_broadcast(uinput_dev, &ie, NULL);
    return 0;
}

void libevdev_uinput_destroy(struct libevdev_uinput* uinput_dev) {
    server_close(uinput_dev);
    free(uinput_dev);
}

int libevdev_event_code_from_name(unsigned int type, const char *name) {
    // TODO: Possibly implement this, Sunshine only uses it for UTF-8 input which is basically never used
    return -1;
}
