#pragma once

#define _GNU_SOURCE
#define	_SYS_IOCTL_H 1
#include <bits/ioctls.h>
#include <bits/ioctl-types.h>
#include <linux/input.h>

#include "macros.h"

struct fakedev_t {
    struct input_id id;
    char name[64];
    char uniq[64];

    unsigned long propbit[BITS_TO_LONGS(INPUT_PROP_CNT)];
    unsigned long evbit[BITS_TO_LONGS(EV_CNT)];
    unsigned long keybit[BITS_TO_LONGS(KEY_CNT)];
    unsigned long relbit[BITS_TO_LONGS(REL_CNT)];
    unsigned long absbit[BITS_TO_LONGS(ABS_CNT)];
    unsigned long mscbit[BITS_TO_LONGS(MSC_CNT)];
    unsigned long ledbit[BITS_TO_LONGS(LED_CNT)];
    unsigned long sndbit[BITS_TO_LONGS(SND_CNT)];
    unsigned long ffbit[BITS_TO_LONGS(FF_CNT)];
    unsigned long swbit[BITS_TO_LONGS(SW_CNT)];

    struct input_absinfo absinfo[ABS_CNT];
};
