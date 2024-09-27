/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_ATOMIC_H
#define _EVL_ATOMIC_H

#include <linux/types.h>
#include <stdatomic.h>
#include <errno.h>

typedef int atomic_t;
typedef unsigned int uatomic_t;

#if !defined(__cplusplus)
#include <stdbool.h>
#endif

#define atomic_read(__ptr)	__atomic_load_n(__ptr, __ATOMIC_ACQUIRE)

#define atomic_cmpxchg(__ptr, __oldval, __newval)			\
	({								\
		typeof(__oldval) __exp = (__oldval);			\
		typeof(__newval) __des = (__newval);			\
		__atomic_compare_exchange_n(				\
			__ptr, &__exp, __des, true,			\
			__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);		\
		__exp;							\
	})

#define atomic_cmpxchg_weak(__ptr, __oldval, __newval)			\
	({								\
		typeof(__oldval) __exp = (__oldval);			\
		typeof(__newval) __des = (__newval);			\
		__atomic_compare_exchange_n(				\
			__ptr, &__exp, __des, true,			\
			__ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);		\
		__exp;							\
	})

#endif /* _EVL_ATOMIC_H */
