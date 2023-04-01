#!/bin/sh
set -ex
gcc -Wall -Werror -fPIC -ldl -shared -o /tmp/fakedev.so *.c
LD_PRELOAD=/tmp/fakedev.so evtest /dev/fakedev
