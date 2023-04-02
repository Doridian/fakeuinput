#pragma once

#define _GNU_SOURCE
#define	_SYS_IOCTL_H 1
#include <bits/ioctls.h>
#include <bits/ioctl-types.h>

#include <linux/input.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/xattr.h>

#include <sys/socket.h>
#include <sys/un.h>

#define FAKEDEV_PATH "/tmp/fakedev/"
#define FAKEDEV_XATTR_NAME "user.fakeuinput.fakedev"

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define BITS_PER_BYTE		8
#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))

#define BITS_PER_LONG (sizeof(long) * BITS_PER_BYTE)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)
#define set_bit(bit, array)	    ((array[LONG(bit)] |= BIT(bit)))
#define clear_bit(bit, array)	((array[LONG(bit)] &= ~BIT(bit)))

struct fakedev_t {
    bool initialized;
    struct input_id id;
    char name[64];

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
