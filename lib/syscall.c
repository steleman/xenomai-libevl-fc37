/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <stdarg.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <errno.h>
#include <evl/syscall.h>
#include <evl/syscall-evl.h>
#include <asm-generic/dovetail.h>

/*
 * EVL relies on Dovetail's handling of prctl(2) to receive requests
 * which have the out-of-band syscall bit set in the option
 * argument. All EVL syscall return values fit in prctl's return type,
 * which is a common integer.
 */
#define __evl_syscall(__nr, __a0, __a1, __a2)	\
	prctl((__nr) | __OOB_SYSCALL_BIT, (long)(__a0), (long)(__a1), (long)(__a2), 0)

ssize_t oob_read(int efd, void *buf, size_t count)
{
	int old_type;
	ssize_t ret;

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old_type);
	ret = __evl_syscall(sys_evl_read, efd, buf, count);
	pthread_setcanceltype(old_type, NULL);

	return ret;
}

ssize_t oob_write(int efd, const void *buf, size_t count)
{
	int old_type;
	ssize_t ret;

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old_type);
	ret = __evl_syscall(sys_evl_write, efd, buf, count);
	pthread_setcanceltype(old_type, NULL);

	return ret;
}

int oob_ioctl(int efd, unsigned long request, ...)
{
	int ret, old_type;
	va_list ap;
	long arg;

	va_start(ap, request);
	arg = va_arg(ap, long);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old_type);
	ret = __evl_syscall(sys_evl_ioctl, efd, request, arg);
	pthread_setcanceltype(old_type, NULL);
	va_end(ap);

	return ret;
}
