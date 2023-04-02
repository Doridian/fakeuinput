#pragma once

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define errlog(str) { fprintf(stderr, "fakedevmgr: " str "\n"); fflush(stderr); }
#define errlogf(str, ...) { fprintf(stderr, "fakedevmgr: " str "\n", __VA_ARGS__); fflush(stderr); }

int set_socket_timeouts(int sockfd);
