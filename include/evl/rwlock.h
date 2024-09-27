/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2022 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_RWLOCK_H
#define _EVL_RWLOCK_H

#include <stdint.h>
#include <evl/flags.h>

struct evl_rwlock {
	unsigned int magic;
	union {
		uatomic_t lock;
		union  __evl_rwlock_inner {
			uint32_t value;
			struct {
				int32_t count: 31;
				int32_t rdpend: 1;
			} u;
		} inner;
	} u;
	struct evl_flags event;
};

#define __RWLOCK_UNINIT_MAGIC	0xc7c7e8e8

#define __EVL_RWLOCK_WRBIAS 0x3fffffff /* 2^30-1 */

#define EVL_RWLOCK_INITIALIZER()					\
  	(struct evl_rwlock)  {						\
		.magic = __RWLOCK_UNINIT_MAGIC,				\
		.u = {							\
			.inner = {					\
				.u = {					\
					.count = __EVL_RWLOCK_WRBIAS,	\
					.rdpend = 0,			\
				}					\
			}						\
		}							\
	}

#define DEFINE_EVL_RWLOCK(__name)	\
	struct evl_rwlock __name = EVL_RWLOCK_INITIALIZER()

#define evl_new_rwlock(__rwlock)	evl_create_rwlock(__rwlock)

#ifdef __cplusplus
extern "C" {
#endif

int evl_create_rwlock(struct evl_rwlock *rwlock);

int evl_destroy_rwlock(struct evl_rwlock *rwlock);

int evl_lock_read(struct evl_rwlock *rwlock);

int evl_trylock_read(struct evl_rwlock *rwlock);

int evl_unlock_read(struct evl_rwlock *rwlock);

int evl_lock_write(struct evl_rwlock *rwlock);

int evl_trylock_write(struct evl_rwlock *rwlock);

int evl_unlock_write(struct evl_rwlock *rwlock);

#ifdef __cplusplus
}
#endif

#endif /* _EVL_RWLOCK_H */
