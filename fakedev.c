#include "base.h"

#include <dlfcn.h>

int (*real_ioctl)(int fd, unsigned long request, void *payload);
int (*real_open)(const char* path, int flags);
int (*real_close)(int fd);

/*
struct udev_device;
const char* (*real_udev_device_get_devnode)(struct udev_device* dev);
const char* udev_device_get_devnode(struct udev_device* dev) {
    if (real_udev_device_get_devnode == NULL) {
        real_udev_device_get_devnode = dlsym(RTLD_NEXT, "udev_device_get_devnode");
    }

    if (strchr(dev->syspath, '/') == NULL) {
        return real_udev_device_get_devnode(dev);
    }

    return FAKEDEV_PATH;
}
*/

#define FAKEDEV_MAX 65535

struct fakedev_t* FAKE_DEVICES[FAKEDEV_MAX+1];

int open(const char* path, int flags) {
    if (unlikely(real_open == NULL)) {
        real_open = dlsym(RTLD_NEXT, "open");
    }

    if (likely(strstr(path, FAKEDEV_PATH) == NULL)) {
        return real_open(path, flags);
    }

    struct sockaddr_un un;
    const int fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("fakedev: socket(%s) failed: %s\n", path, strerror(errno));
        return -EINVAL;
    }

    if (unlikely(fd < 0 || fd > FAKEDEV_MAX)) {
        close(fd);
        return -EINVAL;
    }

    un.sun_family = AF_LOCAL;
    strncpy(un.sun_path, path, sizeof(un.sun_path));

    if (connect(fd, (struct sockaddr *)&un, sizeof(un)) < 0) {
        printf("fakedev: connect(%d, %s) failed: %s\n", fd, path, strerror(errno));
        close(fd);
        return -EINVAL;
    }


    struct fakedev_t* dev = malloc(sizeof(struct fakedev_t));
    ssize_t len = read(fd, dev, sizeof(struct fakedev_t));
    if (unlikely(len != sizeof(struct fakedev_t))) {
        printf("fakedev: read(%d, %s) failed to read fakedev_t: %s\n", fd, path, strerror(errno));
        close(fd);
        free(dev);
        return -ENOENT;
    }
    FAKE_DEVICES[fd] = dev;

    return fd;
}

int close(const int fd) {
    if (unlikely(real_close == NULL)) {
        real_close = dlsym(RTLD_NEXT, "close");
    }

    if (unlikely(fd < 0 || fd > FAKEDEV_MAX)) {
        return real_close(fd);
    }

    struct fakedev_t* dev = FAKE_DEVICES[fd];
    if (likely(dev == NULL)) {
        return real_close(fd);
    }
    FAKE_DEVICES[fd] = NULL;
    free(dev);

    return real_close(fd);
}

#define IOCTL_UNIMPL(REQUEST) \
    case REQUEST: \
        printf("ioctl(%d, " #REQUEST ", %p) unhandled\n", fd, payload); \
        return -EINVAL;

#define IOCTL_0_UNIMPL(REQUEST) \
    case REQUEST(0): \
        printf("ioctl(%d, " #REQUEST "(%d), %p) unhandled\n", fd, size, payload); \
        return -EINVAL;

#define EVIOC_MASK_SIZE(nr)	((nr) & ~(_IOC_SIZEMASK << _IOC_SIZESHIFT))

int ioctl(const int fd, const unsigned long request, void *payload) {
    if (unlikely(real_ioctl == NULL)) {
        real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    }

    if (unlikely(fd < 0 || fd > FAKEDEV_MAX)) {
        return real_ioctl(fd, request, payload);
    }
    
    struct fakedev_t *dev = FAKE_DEVICES[fd];
    if (likely(dev == NULL)) {
        return real_ioctl(fd, request, payload);
    }

    switch (request) {
        case EVIOCGVERSION:
            *(int*)payload = EV_VERSION;
            return 0;

        case EVIOCGID:
            memcpy(payload, &dev->id, sizeof(dev->id));
            return 0;

        case EVIOCGRAB:
            if (payload) {
                printf("grabbing %d\n", fd);
            } else {
                printf("releasing %d\n", fd);
            }
            return 0;

        IOCTL_UNIMPL(EVIOCGREP)
	    IOCTL_UNIMPL(EVIOCRMFF)
	    IOCTL_UNIMPL(EVIOCGEFFECTS)
        IOCTL_UNIMPL(EVIOCREVOKE)
        IOCTL_UNIMPL(EVIOCGMASK)
        IOCTL_UNIMPL(EVIOCSMASK)
        IOCTL_UNIMPL(EVIOCSCLOCKID)
        IOCTL_UNIMPL(EVIOCGKEYCODE)
        IOCTL_UNIMPL(EVIOCSKEYCODE)
        IOCTL_UNIMPL(EVIOCGKEYCODE_V2)
        IOCTL_UNIMPL(EVIOCSKEYCODE_V2)
    }

    const int size = _IOC_SIZE(request);

    switch (EVIOC_MASK_SIZE(request)) {
        case EVIOCGNAME(0):
            strncpy(payload, dev->name, size);
            break;

        case EVIOCGPROP(0):
            memcpy(payload, dev->propbit, MIN(BITS_TO_LONGS(INPUT_PROP_CNT), size));
            break;

        IOCTL_0_UNIMPL(EVIOCGMTSLOTS)
        IOCTL_0_UNIMPL(EVIOCGKEY)
        IOCTL_0_UNIMPL(EVIOCGLED)
        IOCTL_0_UNIMPL(EVIOCGSW)
        IOCTL_0_UNIMPL(EVIOCGPHYS)
        IOCTL_0_UNIMPL(EVIOCGUNIQ)

        case EVIOC_MASK_SIZE(EVIOCSFF):
            printf("ioctl(%d, EVIOCSFF(%d), %p) unhandled\n", fd, size, payload);
            return -EINVAL;
    }

	if (_IOC_TYPE(request) != 'E') {
		return -EINVAL;
    }

    if (_IOC_DIR(request) == _IOC_READ) {
        if ((_IOC_NR(request) & ~EV_MAX) == _IOC_NR(EVIOCGBIT(0, 0))) {
            const int type = _IOC_NR(request) & EV_MAX;

            unsigned long *bits;
            int len;
            switch (type) {
                case      0: bits = dev->evbit;  len = EV_MAX;  break;
                case EV_KEY: bits = dev->keybit; len = KEY_MAX; break;
                case EV_REL: bits = dev->relbit; len = REL_MAX; break;
                case EV_ABS: bits = dev->absbit; len = ABS_MAX; break;
                case EV_MSC: bits = dev->mscbit; len = MSC_MAX; break;
                case EV_LED: bits = dev->ledbit; len = LED_MAX; break;
                case EV_SND: bits = dev->sndbit; len = SND_MAX; break;
                case EV_FF:  bits = dev->ffbit;  len = FF_MAX;  break;
                case EV_SW:  bits = dev->swbit;  len = SW_MAX;  break;
                default: return -EINVAL;
            }

            memcpy(payload, bits, MIN(BITS_TO_LONGS(len), size));
            return 0;
        }

		if ((_IOC_NR(request) & ~ABS_MAX) == _IOC_NR(EVIOCGABS(0))) {
            const int type = _IOC_NR(request) & ABS_MAX;
            if (!dev->absinfo) {
                return -EINVAL;
            }
            memcpy(payload, &dev->absinfo[type], MIN(sizeof(struct input_absinfo), size));
            return 0;
        }
        
        return -EINVAL;
    }

    if (_IOC_DIR(request) == _IOC_WRITE) {
        printf("ioctl(%d, IOC_WRITE(%d), %p) unhandled\n", fd, size, payload);
        if ((_IOC_NR(request) & ~ABS_MAX) == _IOC_NR(EVIOCSABS(0))) {
            const int type = _IOC_NR(request) & ABS_MAX;
            printf("ioctl(%d, IOC_READ(%d)->EVIOCSABS(%d), %p) unhandled\n", fd, size, type, payload);
        }

        return -EINVAL;
    }

    return -EINVAL;
}
