#include "types.h"

#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define FAKEDEV_PATH "/dev/fakedev"


struct fakedev_t FAKEDEV = {
    .initialized = false,
    .id = {
        .bustype = BUS_VIRTUAL,
        .vendor = 0x43,
        .product = 0x69,
        .version = 0x1,
    },
    .name = "fakedev",
};

const char* udev_device_get_devnode(struct udev_device* dev) {
    return FAKEDEV_PATH;
}

int (*real_ioctl)(int fd, unsigned long request, void *payload);

int (*real_open)(const char*, int flags);

int open(const char* path, int flags) {
    if (real_open == NULL) {
        real_open = dlsym(RTLD_NEXT, "open");
    }

    if (strcmp(path, FAKEDEV_PATH) != 0) {
        return real_open(path, flags);
    }

    return 666;
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
    if (real_ioctl == NULL) {
        real_ioctl = dlsym(RTLD_NEXT, "ioctl");
    }

    if (!FAKEDEV.initialized) {
        set_bit(EV_SYN, FAKEDEV.evbit);
        set_bit(EV_ABS, FAKEDEV.evbit);
        set_bit(EV_KEY, FAKEDEV.evbit);
        FAKEDEV.initialized = true;
    }

    if (fd != 666) {
        return real_ioctl(fd, request, payload);
    }

    switch (request) {
        case EVIOCGVERSION:
            *(int*)payload = EV_VERSION;
            return 0;

        case EVIOCGID:
            memcpy(payload, &FAKEDEV.id, sizeof(FAKEDEV.id));
            return 0;

        IOCTL_UNIMPL(EVIOCGREP)
	    IOCTL_UNIMPL(EVIOCRMFF)
	    IOCTL_UNIMPL(EVIOCGEFFECTS)
    	IOCTL_UNIMPL(EVIOCGRAB)
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
            strncpy(payload, FAKEDEV.name, size);
            break;

        case EVIOCGPROP(0):
            memcpy(payload, FAKEDEV.propbit, MIN(BITS_TO_LONGS(INPUT_PROP_CNT), size));
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
                case      0: bits = FAKEDEV.evbit;  len = EV_MAX;  break;
                case EV_KEY: bits = FAKEDEV.keybit; len = KEY_MAX; break;
                case EV_REL: bits = FAKEDEV.relbit; len = REL_MAX; break;
                case EV_ABS: bits = FAKEDEV.absbit; len = ABS_MAX; break;
                case EV_MSC: bits = FAKEDEV.mscbit; len = MSC_MAX; break;
                case EV_LED: bits = FAKEDEV.ledbit; len = LED_MAX; break;
                case EV_SND: bits = FAKEDEV.sndbit; len = SND_MAX; break;
                case EV_FF:  bits = FAKEDEV.ffbit;  len = FF_MAX;  break;
                case EV_SW:  bits = FAKEDEV.swbit;  len = SW_MAX;  break;
                default: return -EINVAL;
            }

            memcpy(payload, bits, MIN(BITS_TO_LONGS(len), size));
            return 0;
        }

		if ((_IOC_NR(request) & ~ABS_MAX) == _IOC_NR(EVIOCGABS(0))) {
            const int type = _IOC_NR(request) & ABS_MAX;
            printf("ioctl(%d, IOC_READ(%d)->EVIOCGABS(%d), %p) unhandled\n", fd, size, type, payload);
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
