#include "../shared/types.h"
#include "types.h"
#include "util.h"

#include <stdlib.h>

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
