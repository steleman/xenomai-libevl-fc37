/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _LIB_EVL_INTERNAL_H
#define _LIB_EVL_INTERNAL_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <evl/thread.h>

#define __evl_ptr64(__ptr)	((__u64)(uintptr_t)(__ptr))

#if __WORDSIZE == 64 || defined(__USE_TIME_BITS64)
/*
 * If timespec is y2038-safe, we don't need to bounce via an
 * __evl_timespec buffer, since both types are guaranteed compatible
 * bitwise. y2038-safe *libc such as glibc and musl set
 * __USE_TIME_BITS64 as required by the uapi contract with the kernel
 * [1], others (including earlier releases) might not so we check
 * __WORDSIZE too in order to always have it right for 64bit
 * architectures.
 *
 * CAUTION: the assumption here is that both the y2038-safe timespec
 * type and __evl_timespec are compatible bitwise with
 * __kernel_timespec, so we may coerce values safely between these
 * types.
 *
 * [1] https://sourceware.org/glibc/wiki/Y2038ProofnessDesign
 */
#define __evl_ktimespec(__ts, __kts)			\
	({						\
		(void)__kts;				\
		(struct __evl_timespec *)__ts;		\
	})
#define __evl_kitimerspec(__its, __kits)		\
	({						\
		(void)__kits;				\
		(struct __evl_itimerspec *)__its;	\
	})
#else
/*
 * Bummer, we have to bounce the 32bit timespec to a 64bit one the
 * kernel would accept. Downside of it: we might get SIGSEGV if __ts
 * is an invalid pointer instead of -EFAULT as one would
 * expect. Upside: this interface won't break user code which did not
 * switch to timespec64, which would be the only reasonable thing to
 * do when support for y2038 is generally available from *libc.
 */
#define __evl_ktimespec(__ts, __kts)			\
	({						\
		__kts.tv_sec = (__ts)->tv_sec;		\
		__kts.tv_nsec = (__ts)->tv_nsec;	\
		&__kts;					\
	})
#define __evl_kitimerspec(__its, __kits)				\
	({								\
		struct __evl_itimerspec *__kitp = NULL;			\
		struct __evl_timespec __kts;				\
		if (__its) {						\
			__kits.it_value = *__evl_ktimespec(		\
				&(__its)->it_value, __kts);		\
			__kits.it_interval = *__evl_ktimespec(		\
				&(__its)->it_interval, __kts);		\
			__kitp = &__kits;				\
		}							\
		__kitp;							\
	})
#endif

#define __evl_ktimespec_ptr64(__ts, __kts)	\
	__evl_ptr64(__evl_ktimespec(__ts, __kts))

#define __evl_kitimerspec_ptr64(__its, __kits)	\
	__evl_ptr64(__evl_kitimerspec(__its, __kits))

/* Enable dlopen() on libevl.so. */
#define EVL_TLS_MODEL	"global-dynamic"

extern __thread __attribute__ ((tls_model (EVL_TLS_MODEL)))
fundle_t __evl_current;

extern __thread __attribute__ ((tls_model (EVL_TLS_MODEL)))
int __evl_current_efd;

extern __thread __attribute__ ((tls_model (EVL_TLS_MODEL)))
struct evl_user_window *__evl_current_window;

static inline int __evl_get_current_mode(void)
{
	return __evl_current_window ?
		__evl_current_window->state : EVL_T_INBAND;
}

static inline fundle_t __evl_get_current(void)
{
	return __evl_current;
}

static inline struct evl_user_window *
__evl_get_current_window(void)
{
	return __evl_current ? __evl_current_window : NULL;
}

static inline bool __evl_is_inband(void)
{
	return !!(__evl_get_current_mode() & EVL_T_INBAND);
}

#define __evl_conforming_io(__efd, __call, __args...)		\
	({							\
		int __ret;					\
		if (__evl_is_inband())				\
			__ret = __call(__efd, ##__args);	\
		else						\
			__ret = oob_##__call(__efd, ##__args);	\
		__ret ? -errno : 0;				\
	})

int __evl_arch_init(void);

int __evl_attach_clocks(void);

void __evl_setup_proxies(void);

extern int (*__evl_clock_gettime)(clockid_t clk_id,
				struct timespec *tp);

extern void *__evl_shared_memory;

extern int __evl_ctlfd;

extern int __evl_mono_clockfd;

extern int __evl_real_clockfd;

#endif /* _LIB_EVL_INTERNAL_H */
