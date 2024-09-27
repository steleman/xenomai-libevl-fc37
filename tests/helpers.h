/*
 * SPDX-License-Identifier: MIT
 */

#ifndef _EVL_TESTS_HELPERS_H
#define _EVL_TESTS_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <linux/ioctl.h>
#include <evl/proxy.h>
#include <evl/proxy-evl.h>

#define EXIT_NO_SUPPORT  42
#define EXIT_NO_STATUS   43

#define ONE_BILLION	1000000000

#define __stringify_1(x...)	#x
#define __stringify(x...)	__stringify_1(x)

#define warn_failed(__fmt, __args...)			\
	evl_eprintf("%s:%d: FAILED: " __fmt "\n",	\
			__FILE__, __LINE__, ##__args)

#define emit_info(__fmt, __args...)			\
	evl_eprintf(__fmt "\n", ##__args)

static inline int abort_test(int status)
{
	exit(status == -EOPNOTSUPP ? EXIT_NO_SUPPORT : 1);
}

#define __Tcall(__ret, __call)				\
	({						\
		(__ret) = (__call);			\
		if (__ret < 0) {			\
			warn_failed("%s (=%s)",		\
				__stringify(__call),	\
				strerror(-(__ret)));	\
		}					\
		(__ret) >= 0;				\
	})

#define __Tcall_assert(__ret, __call)		\
	do {					\
		if (!__Tcall(__ret, __call))	\
			abort_test(__ret);	\
	} while (0)

#define __Fcall(__ret, __call)				\
	({						\
		(__ret) = (__call);			\
		if ((__ret) >= 0)			\
			warn_failed("%s (%d >= 0)",	\
				__stringify(__call),	\
				__ret);			\
		(__ret) < 0;				\
	})

#define __Fcall_assert(__ret, __call)		\
	do {					\
		if (!__Fcall(__ret, __call))	\
			abort_test(__ret);	\
	} while (0)

#define __Tcall_errno(__ret, __call)			\
	({						\
		(__ret) = (__call);			\
		if (__ret < 0)				\
			warn_failed("%s (=%s)",		\
				__stringify(__call),	\
				strerror(errno));	\
		(__ret) >= 0;				\
	})

#define __Tcall_errno_assert(__ret, __call)		\
	do {						\
		(__ret) = (__call);			\
		if (__ret < 0) {			\
			int __errval = errno;		\
			warn_failed("%s (=%s)",		\
				__stringify(__call),	\
				strerror(__errval));	\
			abort_test(-__errval);		\
		}					\
	} while (0)

#define __Texpr(__expr)					\
	({						\
		int __ret = !!(__expr);			\
		if (!__ret)				\
			warn_failed("%s (=false)",	\
				__stringify(__expr));	\
		__ret;					\
	})

#define __Texpr_assert(__expr)		\
	do {				\
		if (!__Texpr(__expr))	\
			abort_test(0);	\
	} while (0)

#define __Fexpr(__expr)					\
	({						\
		int __ret = (__expr);			\
		if (__ret)				\
			warn_failed("%s (=true)",	\
				__stringify(__expr));	\
		!__ret;					\
	})

#define __Fexpr_assert(__expr)		\
	do {				\
		if (!__Fexpr(__expr))	\
			abort_test(0);	\
	} while (0)

char *get_unique_name_and_path(const char *type,
		int serial, char **ppath);

static inline char *get_unique_name(const char *type,
				int serial)
{
	return get_unique_name_and_path(type, serial, NULL);
}

void new_thread(pthread_t *tid, int policy, int prio,
		void *(*fn)(void *), void *arg);

void timespec_add_ns(struct timespec *__restrict r,
		const struct timespec *__restrict t,
		long ns);

long timespec_sub_ns(const struct timespec *__restrict r,
		const struct timespec *__restrict t);

int pick_test_cpu(int hint_cpu,
		bool inband_test, bool *isolated);

#endif /* !_EVL_TESTS_HELPERS_H */
