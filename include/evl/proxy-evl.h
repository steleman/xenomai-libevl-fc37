/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_PROXY_EVL_H
#define _EVL_PROXY_EVL_H

#include <stdarg.h>
#include <sys/types.h>
#include <linux/types.h>
#include <stdio.h>
#include <evl/proxy.h>
#include <evl/factory.h>

#define evl_new_proxy(__targetfd, __bufsz, __fmt, __args...)		\
	evl_create_proxy(__targetfd, __bufsz, 0, EVL_CLONE_PRIVATE,	\
			__fmt, ##__args)

#ifdef __cplusplus
extern "C" {
#endif

int evl_create_proxy(int targetfd, size_t bufsz,
		size_t granularity, int flags,
		const char *fmt, ...);

ssize_t evl_write_proxy(int proxyfd,
		const void *buf, size_t count);

ssize_t evl_read_proxy(int proxyfd,
		void *buf, size_t count);

ssize_t evl_vprint_proxy(int proxyfd,
			const char *fmt, va_list ap);

ssize_t evl_print_proxy(int proxyfd,
			const char *fmt, ...);

ssize_t evl_printf(const char *fmt, ...);

ssize_t evl_eprintf(const char *fmt, ...);

int evl_stdout(void);

int evl_stderr(void);

#ifdef __cplusplus
}
#endif

#endif /* _EVL_PROXY_H */
