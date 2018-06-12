/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  Copyright 2013 Lennart Poettering
***/

int asynchronous_job(void* (*func)(void *p), void *arg);

int asynchronous_sync(pid_t *ret_pid);
int asynchronous_close(int fd);
