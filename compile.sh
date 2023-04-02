#!/bin/sh
set -ex

rm -rf /tmp/fakedev
mkdir -p /tmp/fakedev

gcc -g -Wall -Werror -fPIC -ldl -shared -o /tmp/fakedev.so fakedev.c
gcc -g -Wall -Werror -fPIC -shared -I/usr/include/libevdev-1.0 -o /tmp/fakedevmgr.so fakedevmgr.c

#/tmp/fakedevmgr /tmp/fakedev/js1 &
#sleep 1

/usr/bin/sunshine

#LD_PRELOAD=/tmp/fakedev.so evtest /tmp/fakedev/js1
