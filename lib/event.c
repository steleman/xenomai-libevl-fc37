/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <sched.h>
#include <evl/compiler.h>
#include <evl/atomic.h>
#include <evl/sys.h>
#include <evl/mutex.h>
#include <evl/event.h>
#include <evl/thread.h>
#include <evl/syscall.h>
#include <evl/syscall-evl.h>
#include <linux/types.h>
#include "internal.h"

#define __EVENT_ACTIVE_MAGIC	0xef55ef55
#define __EVENT_DEAD_MAGIC	0

static int init_event_vargs(struct evl_event *evt,
			int clockfd, int flags,
			const char *fmt, va_list ap)
{
	struct evl_monitor_attrs attrs;
	struct evl_element_ids eids;
	char *name = NULL;
	int efd, ret;

	if (__evl_shared_memory == NULL)
		return -ENXIO;

	if (fmt) {
		ret = vasprintf(&name, fmt, ap);
		if (ret < 0)
			return -ENOMEM;
	}

	attrs.type = EVL_MONITOR_EVENT;
	attrs.protocol = EVL_EVENT_GATED;
	attrs.clockfd = clockfd;
	attrs.initval = 0;
	efd = evl_create_element(EVL_MONITOR_DEV, name, &attrs, flags, &eids);
	if (name)
		free(name);
	if (efd < 0)
		return efd;

	evt->u.active.state = __evl_shared_memory + eids.state_offset;
	__force_read_access(evt->u.active.state->flags);
	evt->u.active.fundle = eids.fundle;
	evt->u.active.efd = efd;
	evt->magic = __EVENT_ACTIVE_MAGIC;

	return efd;
}

static int init_event_static(struct evl_event *evt,
			int clockfd, int flags,
			const char *fmt, ...)
{
	va_list ap;
	int efd;

	va_start(ap, fmt);
	efd = init_event_vargs(evt, clockfd, flags, fmt, ap);
	va_end(ap);

	return efd;
}

static int open_event_vargs(struct evl_event *evt,
			const char *fmt, va_list ap)
{
	struct evl_monitor_binding bind;
	int ret, efd;

	efd = evl_open_element_vargs(EVL_MONITOR_DEV, fmt, ap);
	if (efd < 0)
		return efd;

	ret = ioctl(efd, EVL_MONIOC_BIND, &bind);
	if (ret) {
		ret = -errno;
		goto fail;
	}

	if (bind.type != EVL_MONITOR_EVENT ||
		bind.protocol != EVL_EVENT_GATED) {
		ret = -EINVAL;
		goto fail;
	}

	evt->u.active.state = __evl_shared_memory + bind.eids.state_offset;
	__force_read_access(evt->u.active.state->flags);
	evt->u.active.fundle = bind.eids.fundle;
	evt->u.active.efd = efd;
	evt->magic = __EVENT_ACTIVE_MAGIC;

	return 0;
fail:
	close(efd);

	return ret;
}

int evl_create_event(struct evl_event *evt,
		int clockfd, int flags, const char *fmt, ...)
{
	va_list ap;
	int efd;

	va_start(ap, fmt);
	efd = init_event_vargs(evt, clockfd, flags, fmt, ap);
	va_end(ap);

	return efd;
}

int evl_open_event(struct evl_event *evt, const char *fmt, ...)
{
	va_list ap;
	int efd;

	va_start(ap, fmt);
	efd = open_event_vargs(evt, fmt, ap);
	va_end(ap);

	return efd;
}

int evl_close_event(struct evl_event *evt)
{
	int efd;

	if (evt->magic == __EVENT_UNINIT_MAGIC)
		return 0;

	if (evt->magic != __EVENT_ACTIVE_MAGIC)
		return -EINVAL;

	efd = evt->u.active.efd;
	evt->u.active.efd = -1;
	compiler_barrier();
	close(efd);

	evt->u.active.fundle = EVL_NO_HANDLE;
	evt->u.active.state = NULL;
	evt->magic = __EVENT_DEAD_MAGIC;

	return 0;
}

static int check_event_sanity(struct evl_event *evt)
{
	int efd;

	if (evt->magic == __EVENT_UNINIT_MAGIC) {
		efd = init_event_static(evt, evt->u.uninit.clockfd,
					evt->u.uninit.flags,
					evt->u.uninit.name);
		if (efd < 0)
			return efd;
	} else if (evt->magic != __EVENT_ACTIVE_MAGIC)
		return -EINVAL;

	return 0;
}

static struct evl_monitor_state *get_lock_state(struct evl_event *evt)
{
	struct evl_monitor_state *est = evt->u.active.state;

	if (est->u.event.gate_offset == EVL_MONITOR_NOGATE)
		return NULL;	/* Nobody waits on @evt */

	return __evl_shared_memory + est->u.event.gate_offset;
}

struct unwait_data {
	struct evl_monitor_unwaitreq ureq;
	int efd;
};

static void unwait_event(void *data)
{
	struct unwait_data *unwait = data;
	int ret;

	do
		ret = oob_ioctl(unwait->efd, EVL_MONIOC_UNWAIT,
				&unwait->ureq);
	while (ret && errno == EINTR);
}

int evl_timedwait_event(struct evl_event *evt,
			struct evl_mutex *mutex,
			const struct timespec *timeout)
{
	struct evl_monitor_waitreq req;
	struct unwait_data unwait;
	struct __evl_timespec kts;
	int ret;

	if (mutex->magic != __MUTEX_ACTIVE_MAGIC)
		return -EINVAL;

	ret = check_event_sanity(evt);
	if (ret)
		return ret;

	req.gatefd = mutex->u.active.efd;
	req.timeout_ptr = __evl_ktimespec_ptr64(timeout, kts);
	unwait.ureq.gatefd = req.gatefd;
	unwait.efd = evt->u.active.efd;

	for (;;) {
		req.status = -EINVAL;
		req.value = 0;		/* dummy */
		pthread_cleanup_push(unwait_event, &unwait);
		ret = oob_ioctl(evt->u.active.efd, EVL_MONIOC_WAIT, &req);
		pthread_cleanup_pop(0);

		if (!ret || errno == EIDRM)
			return req.status;

		/*
		 * If oob_ioctl() failed for any reason but EIDRM, the
		 * event is still valid but was left unguarded on
		 * return from WAIT: issue UNWAIT to recover and grab
		 * the mutex back.
		 */
		unwait_event(&unwait);

		/*
		 * This should never happen, but in case it does let's
		 * go for robustness and report back.
		 */
		if (errno != EINTR)
			return -errno;

		/*
		 * If oob_ioctl() failed with EINTR, we either:
		 *
		 * - received a signal while waiting for the event
		 * (req.status == 0, SA_RESTART is disabled for the
		 * WAIT request). We retry the wait loop.
		 *
		 * - got forcibly unblocked while waiting for the
		 * event (req.status == -EINTR). The call returns
		 * immediately with the same status.
		 *
		 * - received a signal or got forcibly unblocked while
		 * trying to reacquire the lock once the event was
		 * successfully received (req.status ==
		 * -EAGAIN). Since UNWAIT was performed already, the
		 * call is deemed successful.
		 */
		if (req.status)
			return req.status == -EAGAIN ? 0 : req.status;
	}
}

int evl_wait_event(struct evl_event *evt, struct evl_mutex *mutex)
{
	struct timespec timeout = { .tv_sec = 0, .tv_nsec = 0 };

	return evl_timedwait_event(evt, mutex, &timeout);
}

int evl_signal_event(struct evl_event *evt)
{
	struct evl_monitor_state *est, *gst;
	int ret;

	ret = check_event_sanity(evt);
	if (ret)
		return ret;

	gst = get_lock_state(evt);
	if (gst) {
		gst->flags |= EVL_MONITOR_SIGNALED;
		est = evt->u.active.state;
		est->flags |= EVL_MONITOR_SIGNALED;
	}

	return 0;
}

int evl_signal_thread(struct evl_event *evt, int thrfd)
{
	struct evl_monitor_state *gst;
	__u32 efd;
	int ret;

	ret = check_event_sanity(evt);
	if (ret)
		return ret;

	gst = get_lock_state(evt);
	if (gst) {
		gst->flags |= EVL_MONITOR_SIGNALED;
		efd = evt->u.active.efd;
		return oob_ioctl(thrfd, EVL_THRIOC_SIGNAL, &efd) ? -errno : 0;
	}

	/* No thread waits on @evt, so @thrfd neither => nop. */

	return 0;
}

int evl_broadcast_event(struct evl_event *evt)
{
	struct evl_monitor_state *est, *gst;
	int ret;

	ret = check_event_sanity(evt);
	if (ret)
		return ret;

	gst = get_lock_state(evt);
	if (gst) {
		gst->flags |= EVL_MONITOR_SIGNALED;
		est = evt->u.active.state;
		est->flags |= EVL_MONITOR_SIGNALED|EVL_MONITOR_BROADCAST;
	}

	return 0;
}
