#!/bin/sh
set -ex

rm -rf /tmp/fakedev
mkdir -p /tmp/fakedev

bash -c "cd fakedev && exec gcc -g -Wall -Werror -fPIC -ldl -shared -o /tmp/fakedev.so *.c"
bash -c "cd fakedevmgr && exec gcc -g -Wall -Werror -fPIC -shared -I/usr/include/libevdev-1.0 -o /tmp/fakedevmgr.so *.c"

#/tmp/fakedevmgr /tmp/fakedev/js1 &
#sleep 1

sudo -E LD_PRELOAD=/tmp/fakedevmgr.so /usr/bin/sunshine

#LD_PRELOAD=/tmp/fakedev.so evtest /tmp/fakedev/js1
