/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <evl/sys.h>
#include <evl/proxy.h>
#include <evl/proxy-evl.h>
#include <evl/thread.h>
#include <evl/thread-evl.h>
#include <evl/syscall.h>
#include <evl/syscall-evl.h>
#include "internal.h"

#define STDSTREAM_BUFSZ  16384

static __thread __attribute__ ((tls_model (EVL_TLS_MODEL)))
char fmt_buf[1024];

static int evl_outfd = -EBADF, evl_errfd = -EBADF;

void __evl_setup_proxies(void)
{
	/*
	 * This might fail if stdout/stderr are closed, just ignore if
	 * so.
	 */
	evl_outfd = evl_new_proxy(fileno(stdout), STDSTREAM_BUFSZ,
		"stdout:%d", getpid());
	evl_errfd = evl_new_proxy(fileno(stderr), STDSTREAM_BUFSZ,
		"stderr:%d", getpid());
}

int evl_create_proxy(int targetfd, size_t bufsz, size_t granularity,
		int flags, const char *fmt, ...)
{
	struct evl_proxy_attrs attrs;
	char *name = NULL;
	int ret, efd;
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
		ret = vasprintf(&name, fmt, ap);
		va_end(ap);
		if (ret < 0)
			return -ENOMEM;
	}

	attrs.fd = targetfd;
	attrs.bufsz = bufsz;
	attrs.granularity = granularity;
	efd = evl_create_element(EVL_PROXY_DEV, name, &attrs, flags, NULL);
	if (name)
		free(name);

	return efd;
}

ssize_t evl_write_proxy(int proxyfd, const void *buf, size_t count)
{
	ssize_t ret;

	ret = __evl_conforming_io(proxyfd, write, buf, count);

	return ret < 0 ? -errno : ret;
}

ssize_t evl_read_proxy(int proxyfd, void *buf, size_t count)
{
	ssize_t ret;

	ret = __evl_conforming_io(proxyfd, read, buf, count);

	return ret < 0 ? -errno : ret;
}

ssize_t evl_vprint_proxy(int proxyfd, const char *fmt, va_list ap)
{
	ssize_t count = vsnprintf(fmt_buf, sizeof(fmt_buf), fmt, ap), ret;

	if (count < 0)
		return -errno;	/* assume POSIX behavior. */

	/*
	 * If evl_init() has not run yet, we can resort to vprintf()
	 * since latency should not be an issue.
	 */
	if (proxyfd < 0) {
		if (evl_is_inband()) {
			ret = write(2, fmt_buf, count);
			if (ret < 0)
				ret = -errno;
		} else {
			ret = -EBADFD;
		}
	} else {
		ret = evl_write_proxy(proxyfd, fmt_buf, count);
	}

	return ret;
}

ssize_t evl_print_proxy(int proxyfd, const char *fmt, ...)
{
	ssize_t ret;
	va_list ap;

	va_start(ap, fmt);
	ret = evl_vprint_proxy(proxyfd, fmt, ap);
	va_end(ap);

	return ret;
}

ssize_t evl_printf(const char *fmt, ...)
{
	ssize_t ret;
	va_list ap;

	va_start(ap, fmt);
	ret = evl_vprint_proxy(evl_outfd, fmt, ap);
	va_end(ap);

	return ret;
}

ssize_t evl_eprintf(const char *fmt, ...)
{
	ssize_t ret;
	va_list ap;

	va_start(ap, fmt);
	ret = evl_vprint_proxy(evl_errfd, fmt, ap);
	va_end(ap);

	return ret;

}

int evl_stdout(void)
{
	return evl_outfd;
}

int evl_stderr(void)
{
	return evl_errfd;
}
