/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_FLAGS_H
#define _EVL_FLAGS_H

#include <time.h>
#include <evl/atomic.h>
#include <evl/types.h>
#include <evl/monitor.h>
#include <evl/clock.h>
#include <evl/factory.h>

struct evl_flags {
	unsigned int magic;
	union {
		struct {
			const char *name;
			int clockfd;
			int initval;
			int flags;
		} uninit;
		struct {
			fundle_t fundle;
			struct evl_monitor_state *state;
			int efd;
		} active;
	} u;
};

#define __FLAGS_UNINIT_MAGIC	0xfebcfebc

#define EVL_FLAGS_INITIALIZER(__name, __clockfd, __initval, __flags)	\
	(struct evl_flags) {						\
		.magic = __FLAGS_UNINIT_MAGIC,				\
		.u = {							\
			.uninit = {					\
				.name = (__name),			\
				.clockfd = (__clockfd),			\
				.initval = (__initval),			\
				.flags = (__flags),			\
			}						\
		}							\
	}

#define DEFINE_EVL_FLAGS(__name)				\
  	struct evl_flags __name =				\
	  EVL_FLAGS_INITIALIZER(#__name, EVL_CLOCK_MONOTONIC,	\
				0, EVL_CLONE_PRIVATE)

#define evl_new_flags(__flg, __fmt, __args...)		    \
	evl_create_flags(__flg, EVL_CLOCK_MONOTONIC, 0,	    \
			EVL_CLONE_PRIVATE, __fmt, ##__args)

#ifdef __cplusplus
extern "C" {
#endif

int evl_create_flags(struct evl_flags *flg,
		int clockfd, int initval, int flags,
		const char *fmt, ...);

int evl_open_flags(struct evl_flags *flg,
		const char *fmt, ...);

int evl_close_flags(struct evl_flags *flg);

int evl_timedwait_some_flags(struct evl_flags *flg, int bits,
			const struct timespec *timeout,
			int *r_bits);

int evl_timedwait_exact_flags(struct evl_flags *flg, int bits,
			const struct timespec *timeout);

int evl_timedwait_flags(struct evl_flags *flg,
			const struct timespec *timeout,
			int *r_bits);

int evl_wait_some_flags(struct evl_flags *flg,
			int bits, int *r_bits);

int evl_wait_exact_flags(struct evl_flags *flg,
			int bits);

int evl_wait_flags(struct evl_flags *flg,
		int *r_bits);

int evl_trywait_some_flags(struct evl_flags *flg,
			int bits, int *r_bits);

int evl_trywait_exact_flags(struct evl_flags *flg,
			int bits);

int evl_trywait_flags(struct evl_flags *flg,
		int *r_bits);

int evl_post_flags(struct evl_flags *flg,
		int bits);

int evl_broadcast_flags(struct evl_flags *flg,
			int bits);

int evl_peek_flags(struct evl_flags *flg,
		int *r_bits);

#ifdef __cplusplus
}
#endif

#endif /* _EVL_FLAGS_H */
