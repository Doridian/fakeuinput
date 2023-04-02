#include "../shared/base.h"

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

// TODO: Map or something
#define FAKEDEV_MAX 65535

#define errlog(str, ...) fprintf(stderr, "fakedev: " str "\n")
#define errlogf(str, ...) fprintf(stderr, "fakedev: " str "\n", __VA_ARGS__)

struct fakedev_t* FAKE_DEVICES[FAKEDEV_MAX+1];

static struct fakedev_t* get_fake_device(int fd) {
    if (unlikely(fd < 0 || fd > FAKEDEV_MAX)) {
        return NULL;
    }
    return FAKE_DEVICES[fd];
}

static int set_fake_device(int fd, struct fakedev_t* dev) {
    if (unlikely(fd < 0 || fd > FAKEDEV_MAX || dev == NULL)) {
        close(fd);
        return -EINVAL;
    }
    FAKE_DEVICES[fd] = dev;
    return fd;
}

static void delete_fake_device(int fd) {
    if (unlikely(fd < 0 || fd > FAKEDEV_MAX)) {
        return;
    }
    free(FAKE_DEVICES[fd]);
    FAKE_DEVICES[fd] = NULL;
}
// END: TODO

int open(const char* path, int flags) {
    if (unlikely(real_open == NULL)) {
        real_open = dlsym(RTLD_NEXT, "open");
    }

    if (likely(strstr(path, FAKEDEV_PATH) == NULL)) {
        return real_open(path, flags);
    }

    struct sockaddr_un un;
    const int fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (unlikely(fd < 0)) {
        errlogf("socket(%s) failed: %s", path, strerror(errno));
        return -EINVAL;
    }

    un.sun_family = AF_LOCAL;
    strncpy(un.sun_path, path, sizeof(un.sun_path));

    if (unlikely(connect(fd, (struct sockaddr *)&un, sizeof(un)) < 0)) {
        errlogf("connect(%d, %s) failed: %s", fd, path, strerror(errno));
        close(fd);
        return -EINVAL;
    }


    struct fakedev_t* dev = malloc(sizeof(struct fakedev_t));
    ssize_t len = read(fd, dev, sizeof(struct fakedev_t));
    if (unlikely(len != sizeof(struct fakedev_t))) {
        errlogf("read(%d, %s) failed to read fakedev_t: %s", fd, path, strerror(errno));
        close(fd);
        free(dev);
        return -EINVAL;
    }

    return set_fake_device(fd, dev);
}

int close(const int fd) {
    if (unlikely(real_close == NULL)) {
        real_close = dlsym(RTLD_NEXT, "close");
    }

    delete_fake_device(fd);
    return real_close(fd);
}

#define IOCTL_UNIMPL(REQUEST) \
    case REQUEST: \
        errlogf("ioctl(%d, " #REQUEST ", %p) unhandled", fd, payload); \
        return -EINVAL;

#define IOCTL_0_UNIMPL(REQUEST) \
    case REQUEST(0): \
        errlogf("ioctl(%d, " #REQUEST "(%d), %p) unhandled", fd, size, payload); \
        return -EINVAL;

#define EVIOC_MASK_SIZE(nr)	((nr) & ~(_IOC_SIZEMASK << _IOC_SIZESHIFT))

int ioctl(const int fd, const unsigned long request, void *payload) {
    if (unlikely(real_ioctl == NULL)) {
        real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    }
    
    struct fakedev_t *dev = get_fake_device(fd);
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
                errlogf("grabbing %d", fd);
            } else {
                errlogf("releasing %d", fd);
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

        case EVIOCGUNIQ(0):
            strncpy(payload, dev->uniq, size);
            break;

        case EVIOCGPROP(0):
            memcpy(payload, dev->propbit, MIN(BITS_TO_LONGS(INPUT_PROP_CNT), size));
            break;

        IOCTL_0_UNIMPL(EVIOCGMTSLOTS)
        IOCTL_0_UNIMPL(EVIOCGKEY)
        IOCTL_0_UNIMPL(EVIOCGLED)
        IOCTL_0_UNIMPL(EVIOCGSW)
        IOCTL_0_UNIMPL(EVIOCGPHYS)

        case EVIOC_MASK_SIZE(EVIOCSFF):
            errlogf("ioctl(%d, EVIOCSFF(%d), %p) unhandled", fd, size, payload);
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
        if ((_IOC_NR(request) & ~ABS_MAX) == _IOC_NR(EVIOCSABS(0))) {
            const int type = _IOC_NR(request) & ABS_MAX;
            errlogf("ioctl(%d, IOC_READ(%d)->EVIOCSABS(%d), %p) unhandled", fd, size, type, payload);
        }

        return -EINVAL;
    }

    return -EINVAL;
}
