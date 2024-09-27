/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <stdbool.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <evl/compiler.h>
#include <evl/atomic.h>
#include <evl/sys.h>
#include <evl/flags.h>
#include <evl/thread.h>
#include <evl/syscall.h>
#include <evl/syscall-evl.h>
#include <linux/types.h>
#include "internal.h"

#define __FLAGS_ACTIVE_MAGIC	0xb42bb42b
#define __FLAGS_DEAD_MAGIC	0

int evl_create_flags(struct evl_flags *flg, int clockfd,
		int initval, int flags,
		const char *fmt, ...)
{
	struct evl_monitor_attrs attrs;
	struct evl_element_ids eids;
	char *name = NULL;
	int efd, ret;
	va_list ap;

	if (__evl_shared_memory == NULL)
		return -ENXIO;

	if (fmt) {
		va_start(ap, fmt);
		ret = vasprintf(&name, fmt, ap);
		va_end(ap);
		if (ret < 0)
			return -ENOMEM;
	}

	attrs.type = EVL_MONITOR_EVENT;
	attrs.protocol = EVL_EVENT_MASK;
	attrs.clockfd = clockfd;
	attrs.initval = initval;
	efd = evl_create_element(EVL_MONITOR_DEV, name, &attrs,	flags, &eids);
	if (name)
		free(name);
	if (efd < 0)
		return efd;

	flg->u.active.state = __evl_shared_memory + eids.state_offset;
	atomic_store(&flg->u.active.state->u.event.value, initval);
	flg->u.active.fundle = eids.fundle;
	flg->u.active.efd = efd;
	flg->magic = __FLAGS_ACTIVE_MAGIC;

	return efd;
}

int evl_open_flags(struct evl_flags *flg, const char *fmt, ...)
{
	struct evl_monitor_binding bind;
	int ret, efd;
	va_list ap;

	va_start(ap, fmt);
	efd = evl_open_element_vargs(EVL_MONITOR_DEV, fmt, ap);
	va_end(ap);
	if (efd < 0)
		return efd;

	ret = ioctl(efd, EVL_MONIOC_BIND, &bind);
	if (ret) {
		ret = -errno;
		goto fail;
	}

	if (bind.type != EVL_MONITOR_EVENT ||
		bind.protocol != EVL_EVENT_MASK) {
		ret = -EINVAL;
		goto fail;
	}

	flg->u.active.state = __evl_shared_memory + bind.eids.state_offset;
	__force_read_access(flg->u.active.state->u.event.value);
	flg->u.active.fundle = bind.eids.fundle;
	flg->u.active.efd = efd;
	flg->magic = __FLAGS_ACTIVE_MAGIC;

	return efd;
fail:
	close(efd);

	return ret;
}

int evl_close_flags(struct evl_flags *flg)
{
	int ret;

	if (flg->magic == __FLAGS_UNINIT_MAGIC)
		return 0;

	if (flg->magic != __FLAGS_ACTIVE_MAGIC)
		return -EINVAL;

	ret = close(flg->u.active.efd);
	if (ret)
		return -errno;

	flg->u.active.fundle = EVL_NO_HANDLE;
	flg->u.active.state = NULL;
	flg->magic = __FLAGS_DEAD_MAGIC;

	return 0;
}

static int check_sanity(struct evl_flags *flg)
{
	int efd;

	if (flg->magic == __FLAGS_UNINIT_MAGIC) {
		efd = evl_create_flags(flg,
				flg->u.uninit.clockfd,
				flg->u.uninit.initval,
				flg->u.uninit.flags,
				flg->u.uninit.name);
		return efd < 0 ? efd : 0;
	}

	return flg->magic != __FLAGS_ACTIVE_MAGIC ? -EINVAL : 0;
}

static int do_timedwait_flags(struct evl_flags *flg,
			int bits, bool exact_match,
			const struct timespec *timeout,
			int *r_bits)
{
	struct evl_monitor_waitreq req;
	struct __evl_timespec kts;
	fundle_t current;
	int ret;

	current = __evl_get_current();
	if (current == EVL_NO_HANDLE)
		return -EPERM;

	ret = check_sanity(flg);
	if (ret)
		return ret;

	req.gatefd = -1;
	req.timeout_ptr = __evl_ktimespec_ptr64(timeout, kts);
	req.status = -EINVAL;
	req.value = bits;

	ret = oob_ioctl(flg->u.active.efd,
			exact_match ? EVL_MONIOC_WAIT_EXACT :
			EVL_MONIOC_WAIT, &req);
	if (ret)
		return -errno;

	if (req.status)
		return req.status;

	if (r_bits)
		*r_bits = req.value;

	return 0;
}

int evl_timedwait_some_flags(struct evl_flags *flg,
			int bits, const struct timespec *timeout,
			int *r_bits)
{
	return do_timedwait_flags(flg, bits, false, timeout, r_bits);
}

int evl_timedwait_exact_flags(struct evl_flags *flg, int bits,
			const struct timespec *timeout)
{
	return do_timedwait_flags(flg, bits, true, timeout, NULL);
}

int evl_timedwait_flags(struct evl_flags *flg,
			const struct timespec *timeout,
			int *r_bits)
{
	return evl_timedwait_some_flags(flg, -1, timeout, r_bits);
}

int evl_wait_some_flags(struct evl_flags *flg,
			int bits, int *r_bits)
{
	struct timespec timeout = { .tv_sec = 0, .tv_nsec = 0 };

	return evl_timedwait_some_flags(flg, bits, &timeout, r_bits);
}

int evl_wait_exact_flags(struct evl_flags *flg,	int bits)
{
	struct timespec timeout = { .tv_sec = 0, .tv_nsec = 0 };

	return evl_timedwait_exact_flags(flg, bits, &timeout);
}

int evl_wait_flags(struct evl_flags *flg,
		int *r_bits)
{
	return evl_wait_some_flags(flg, -1, r_bits);
}

static int do_trywait_flags(struct evl_flags *flg,
		int bits, bool exact_match,
		int *r_bits)
{
	struct evl_monitor_trywaitreq req;
	int ret, cmd;

	ret = check_sanity(flg);
	if (ret)
		return ret;

	req.value = bits;
	cmd = exact_match ? EVL_MONIOC_TRYWAIT_EXACT :
		EVL_MONIOC_TRYWAIT;

	/*
	 * In-band threads may trywait flags directly, no need to
	 * trigger a stage switch since we won't sleep.
	 */
	if (__evl_get_current() && !__evl_is_inband())
		ret = oob_ioctl(flg->u.active.efd, cmd, &req);
	else
		ret = ioctl(flg->u.active.efd, cmd, &req);
	if (ret)
		return -errno;

	if (r_bits)
		*r_bits = req.value;

	return 0;
}

int evl_trywait_some_flags(struct evl_flags *flg,
			int bits, int *r_bits)
{
	return do_trywait_flags(flg, bits, false, r_bits);
}

int evl_trywait_exact_flags(struct evl_flags *flg, int bits)
{
	return do_trywait_flags(flg, bits, true, NULL);
}

int evl_trywait_flags(struct evl_flags *flg,
		int *r_bits)
{
	return evl_trywait_some_flags(flg, -1, r_bits);
}

static int do_post_flags(struct evl_flags *flg, int bits, bool bcast)
{
	__s32 mask = bits;
	int ret, cmd;

	ret = check_sanity(flg);
	if (ret)
		return ret;

	if (!bits)
		return -EINVAL;

	cmd = bcast ? EVL_MONIOC_BROADCAST : EVL_MONIOC_SIGNAL;

	/* See trywait(). */
	if (__evl_get_current() && !__evl_is_inband())
		ret = oob_ioctl(flg->u.active.efd, cmd, &mask);
	else
		ret = ioctl(flg->u.active.efd, cmd, &mask);

	return ret ? -errno : 0;
}

int evl_post_flags(struct evl_flags *flg, int bits)
{
	return do_post_flags(flg, bits, false);
}

int evl_broadcast_flags(struct evl_flags *flg, int bits)
{
	return do_post_flags(flg, bits, true);
}

int evl_peek_flags(struct evl_flags *flg, int *r_bits)
{
	if (flg->magic != __FLAGS_ACTIVE_MAGIC)
		return -EINVAL;

	*r_bits = atomic_load(&flg->u.active.state->u.event.value);

	return 0;
}
