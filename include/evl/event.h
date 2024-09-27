/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_EVENT_H
#define _EVL_EVENT_H

#include <time.h>
#include <linux/types.h>
#include <evl/atomic.h>
#include <evl/types.h>
#include <evl/mutex.h>
#include <evl/monitor.h>
#include <evl/clock.h>
#include <evl/mutex-evl.h>

struct evl_event {
	unsigned int magic;
	union {
		struct {
			fundle_t fundle;
			struct evl_monitor_state *state;
			int efd;
		} active;
		struct {
			const char *name;
			int clockfd;
			int flags;
		} uninit;
	} u;
};

#define __EVENT_UNINIT_MAGIC	0x01770177

#define EVL_EVENT_INITIALIZER(__name, __clockfd, __flags)  	\
	(struct evl_event) {					\
		.magic = __EVENT_UNINIT_MAGIC,			\
		.u = {						\
			.uninit = {				\
				.name = (__name),		\
				.clockfd = (__clockfd),		\
				.flags = (__flags),		\
			}					\
		}						\
	}

#define DEFINE_EVL_EVENT(__name)				\
  	struct evl_event __name =				\
	  EVL_EVENT_INITIALIZER(#__name, EVL_CLOCK_MONOTONIC,	\
				EVL_CLONE_PRIVATE)

#define evl_new_event(__evt, __fmt, __args...)		\
	evl_create_event(__evt, EVL_CLOCK_MONOTONIC,	\
			EVL_CLONE_PRIVATE, __fmt, ##__args)

#ifdef __cplusplus
extern "C" {
#endif

int evl_create_event(struct evl_event *evt,
		int clockfd, int flags,
		const char *fmt, ...);

int evl_open_event(struct evl_event *evt,
		const char *fmt, ...);

int evl_wait_event(struct evl_event *evt,
		struct evl_mutex *mutex);

int evl_timedwait_event(struct evl_event *evt,
			struct evl_mutex *mutex,
			const struct timespec *timeout);

int evl_signal_event(struct evl_event *evt);

int evl_signal_thread(struct evl_event *evt,
		int thrfd);

int evl_broadcast_event(struct evl_event *evt);

int evl_close_event(struct evl_event *evt);

#ifdef __cplusplus
}
#endif

#endif /* _EVL_EVENT_H */
