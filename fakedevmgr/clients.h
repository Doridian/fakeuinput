#pragma once

#include "types.h"

void fakedevmgr_client_close(struct fakedevmgr_client* client);
void fakedevmgr_client_close_wait(struct fakedevmgr_client* client);
int fakedevmgr_cleanup_client_if_gone(struct fakedevmgr_client* client);
void* fakedevmgr_client_thread(void* arg);
