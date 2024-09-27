/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_POLL_EVL_H
#define _EVL_POLL_EVL_H

#include <sys/types.h>
#include <sys/poll.h>
#include <evl/poll.h>

#ifdef __cplusplus
extern "C" {
#endif

int evl_new_poll(void);

int evl_add_pollfd(int efd, int newfd,
		unsigned int events,
		union evl_value pollval);

int evl_del_pollfd(int efd, int delfd);

int evl_mod_pollfd(int efd, int modfd,
		unsigned int events,
		union evl_value pollval);

int evl_timedpoll(int efd, struct evl_poll_event *pollset,
		int nrset, const struct timespec *timeout);

int evl_poll(int efd, struct evl_poll_event *pollset,
	int nrset);

#ifdef __cplusplus
}
#endif

#endif /* _EVL_POLL_H */
