/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_EVL_H
#define _EVL_EVL_H

#include <signal.h>
#include <evl/clock.h>
#include <evl/mutex.h>
#include <evl/event.h>
#include <evl/sem.h>
#include <evl/flags.h>
#include <evl/syscall.h>
#include <evl/thread.h>
#include <evl/sched.h>
#include <evl/xbuf.h>
#include <evl/poll.h>
#include <evl/proxy.h>
#include <evl/rwlock.h>
#include <evl/control.h>

#define __EVL__  26	/* API version */

#define EVL_ABI_PREREQ  32

#if EVL_ABI_LEVEL < EVL_ABI_PREREQ
#error EVL kernel uapi is too old
#endif

struct evl_version {
	int api_level;	/* libevl.so: __EVL__ */
	int abi_level;	/* EVL_ABI_PREREQ */
	const char *version_string;
};

#ifdef __cplusplus
extern "C" {
#endif

int evl_init(void);

void evl_sigdebug_handler(int sig, siginfo_t *si, void *ctxt);

struct evl_version evl_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* _EVL_EVL_H */
