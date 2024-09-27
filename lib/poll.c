/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <evl/sys.h>
#include <evl/syscall.h>
#include <evl/syscall-evl.h>
#include <evl/poll.h>
#include <evl/poll-evl.h>
#include "internal.h"

int evl_new_poll(void)
{
	return evl_open_raw(EVL_POLL_DEV);
}

static int update_pollset(int efd, int op, int fd, unsigned int events,
			union evl_value pollval)
{
	struct evl_poll_ctlreq creq;
	int ret;

	creq.action = op;
	creq.fd = fd;
	creq.events = events;
	creq.pollval = pollval;
	ret = oob_ioctl(efd, EVL_POLIOC_CTL, &creq);

	return ret ? -errno : 0;
}

int evl_add_pollfd(int efd, int fd, unsigned int events,
		union evl_value pollval)
{
	return update_pollset(efd, EVL_POLL_CTLADD, fd, events, pollval);
}

int evl_del_pollfd(int efd, int fd)
{
	return update_pollset(efd, EVL_POLL_CTLDEL, fd, 0, evl_nil);
}

int evl_mod_pollfd(int efd, int fd,
		unsigned int events, union evl_value pollval)
{
	return update_pollset(efd, EVL_POLL_CTLMOD, fd, events, pollval);
}

static int do_poll(int efd, struct evl_poll_event *pollset,
		int nrset, const struct timespec *timeout)
{
	struct evl_poll_waitreq wreq;
	struct __evl_timespec kts;
	int ret;

	wreq.timeout_ptr = __evl_ktimespec_ptr64(timeout, kts);
	wreq.pollset_ptr = __evl_ptr64(pollset);
	wreq.nrset = nrset;
	ret = oob_ioctl(efd, EVL_POLIOC_WAIT, &wreq);
	if (ret)
		return -errno;

	return wreq.nrset;
}

int evl_timedpoll(int efd, struct evl_poll_event *pollset,
		int nrset, const struct timespec *timeout)
{
	return do_poll(efd, pollset, nrset, timeout);
}

int evl_poll(int efd, struct evl_poll_event *pollset, int nrset)
{
	struct timespec timeout = { .tv_sec = 0, .tv_nsec = 0 };

	return do_poll(efd, pollset, nrset, &timeout);
}
