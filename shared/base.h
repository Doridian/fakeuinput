#pragma once

#define _GNU_SOURCE

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

#include "macros.h"
#include "types.h"
