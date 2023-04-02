#!/bin/sh
set -ex

pkill -9 -fe fakedevmgr || true

mkdir -p /tmp/fakedev

gcc -Wall -Werror -fPIC -ldl -shared -o /tmp/fakedev.so fakedev.c
gcc -Wall -Werror -o /tmp/fakedevmgr fakedevmgr.c

/tmp/fakedevmgr /tmp/fakedev/js1 &
sleep 1

LD_PRELOAD=/tmp/fakedev.so strace evtest /tmp/fakedev/js1
