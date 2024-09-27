/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2022 Philippe Gerum  <rpm@xenomai.org>
 */

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <evl/rwlock.h>

#define __RWLOCK_ACTIVE_MAGIC	0xd8d8f9f9
#define __RWLOCK_DEAD_MAGIC	0

#define __EVL_RWLOCK_RD  0x1
#define __EVL_RWLOCK_WR  0x2

int evl_create_rwlock(struct evl_rwlock *rwlock)
{
	int ret;

	*rwlock = (struct evl_rwlock)EVL_RWLOCK_INITIALIZER();

	ret = evl_new_flags(&rwlock->event, NULL); /* Unnamed private flag. */
	if (ret < 0)
		return ret;

	rwlock->magic = __RWLOCK_ACTIVE_MAGIC;

	return 0;
}

int evl_destroy_rwlock(struct evl_rwlock *rwlock)
{
	if (rwlock->magic == __RWLOCK_UNINIT_MAGIC)
		return 0;

	if (rwlock->magic != __RWLOCK_ACTIVE_MAGIC)
		return -EINVAL;

	rwlock->magic = __RWLOCK_DEAD_MAGIC;

	return evl_close_flags(&rwlock->event);
}

static int check_sanity(struct evl_rwlock *rwlock)
{
	int ret;

	/*
	 * Proceed with lazy init of a statically initialized lock if
	 * needed.
	 */
	if (rwlock->magic == __RWLOCK_UNINIT_MAGIC) {
		ret = evl_new_flags(&rwlock->event, NULL);
		if (ret < 0)
			return ret;
		rwlock->magic = __RWLOCK_ACTIVE_MAGIC;
		return 0;
	}

	return rwlock->magic != __RWLOCK_ACTIVE_MAGIC ? -EINVAL : 0;
}

int evl_lock_read(struct evl_rwlock *rwlock)
{
	union __evl_rwlock_inner prev, next;
	uint32_t oldval;
	int ret;

	ret = check_sanity(rwlock);
	if (ret)
		return ret;

	for (;;) {
		for (;;) {
			prev.value = atomic_read(&rwlock->u.lock);
			if (prev.u.count <= 0) {
				/*
				 * Some writer has the lock, other writers
				 * might be contending for it too.  Readers do
				 * not participate in negative counting so
				 * that only writers do, raise the global
				 * reader-pending flag instead.
				 */
				next.u.rdpend = 1;
				next.u.count = prev.u.count;
			} else {
				/*
				 * The lock is either free, or held by readers
				 * exclusively. Decrement the count to get
				 * access.
				 */
				assert(!prev.u.rdpend);
				next.u.rdpend = 0;
				next.u.count = prev.u.count - 1;
			}

			oldval = atomic_cmpxchg_weak(&rwlock->u.lock, prev.value, next.value);
			if (oldval == prev.value)
				break;
		}

		/*
		 * If no writer was running on entry, grant immediate
		 * access.
		 */
		if (prev.u.count > 0)
			break;
		/*
		 * Well, ok, we have to wait. Unlike writers, readers
		 * do not receive ownership upon wake up, so we have
		 * to redo a full locking attempt once we
		 * resume. Unblocking a thread waiting on a rwlock
		 * should not be allowed, ignore such requests.
		 */
		do {
			ret = evl_wait_exact_flags(&rwlock->event, __EVL_RWLOCK_RD);
		} while (ret && ret == -EINTR);
		if (ret)
			return ret;
	}

	return ret;
}

int evl_trylock_read(struct evl_rwlock *rwlock)
{
	union __evl_rwlock_inner prev, next;
	uint32_t oldval;
	int ret;

	ret = check_sanity(rwlock);
	if (ret)
		return ret;

	prev.value = atomic_read(&rwlock->u.lock);
	if (prev.u.count <= 0)
		return -EAGAIN;

	assert(!prev.u.rdpend);
	next.u.rdpend = 0;
	next.u.count = prev.u.count - 1;
	oldval = atomic_cmpxchg(&rwlock->u.lock, prev.value, next.value);

	return oldval == prev.value ? 0 : -EAGAIN;
}

int evl_unlock_read(struct evl_rwlock *rwlock)
{
	union __evl_rwlock_inner prev, next;
	uint32_t oldval;
	int ret = 0;

	for (;;) {
		prev.value = atomic_read(&rwlock->u.lock);
		next.u.rdpend = prev.u.rdpend;
		next.u.count = prev.u.count + 1;
		oldval = atomic_cmpxchg_weak(&rwlock->u.lock, prev.value, next.value);
		if (oldval == prev.value)
			break;
	}

	/*
	 * If one or more writers were pending on entry
	 * (i.e. prev.u.count < 0), release one of them when the last
	 * reader has released the lock. By bumping the count to zero
	 * in this case, we have also transferred ownership to the
	 * writer which is going to resume, meaning that any other
	 * contending reader/writer which manages to slip in would
	 * have to wait.
	 *
	 * Since readers never block each others, there is no point in
	 * waking them up on a read-side release.
	 */
	if (next.u.count == 0)
		ret = evl_post_flags(&rwlock->event, __EVL_RWLOCK_WR);

	return ret;
}

int evl_lock_write(struct evl_rwlock *rwlock)
{
	union __evl_rwlock_inner prev, next;
	uint32_t oldval;
	int ret;

	ret = check_sanity(rwlock);
	if (ret)
		return ret;

	for (;;) {
		prev.value = atomic_read(&rwlock->u.lock);
		/*
		 * If the count is negative or zero, another writer is
		 * currently holding the lock: decrement by one to
		 * register.
		 *
		 * Otherwise, some or no reader(s) is holding it, in
		 * which case we decrement by the bias value.  As a
		 * result, the count either dropped to zero if we just
		 * grabbed a free lock, or went negative if we have to
		 * wait for some reader thread(s) to release it.
		 */
		if (prev.u.count <= 0)
			next.u.count = prev.u.count - 1;
		else
			next.u.count = prev.u.count - __EVL_RWLOCK_WRBIAS;

		next.u.rdpend = prev.u.rdpend;
		oldval = atomic_cmpxchg_weak(&rwlock->u.lock, prev.value, next.value);
		if (oldval == prev.value)
			break;
	}

	assert(next.u.count <= 0);

	/*
	 *  A single writer may access only when the count drops from
	 *  the writer bias value to zero.  Otherwise we are
	 *  contending with some reader(s) or a single writer
	 *  currently holding the lock.
	 *
	 *  If we cannot lock for write at once, wait for a release
	 *  signal. When resuming from a successful wait, we know that
	 *  have been granted ownership of the lock by the thread
	 *  which released it.
	 */
	if (next.u.count != 0) {
		do {
			ret = evl_wait_exact_flags(&rwlock->event, __EVL_RWLOCK_WR);
		} while (ret && ret == -EINTR);
	}

	return ret;
}

int evl_trylock_write(struct evl_rwlock *rwlock)
{
	union __evl_rwlock_inner prev, next;
	uint32_t oldval;
	int ret;

	ret = check_sanity(rwlock);
	if (ret)
		return ret;

	prev.value = atomic_read(&rwlock->u.lock);
	next.u.count = prev.u.count - __EVL_RWLOCK_WRBIAS;
	assert(next.u.count <= 0);
	if (next.u.count < 0)
		return -EAGAIN;

	next.u.rdpend = prev.u.rdpend;
	oldval = atomic_cmpxchg(&rwlock->u.lock, prev.value, next.value);

	return oldval == prev.value ? 0 : -EAGAIN;
}

int evl_unlock_write(struct evl_rwlock *rwlock)
{
	union __evl_rwlock_inner prev, next;
	uint32_t oldval;
	int ret = 0;

	for (;;) {
		prev.value = atomic_read(&rwlock->u.lock);
		if (prev.u.count < 0) {
			/*
			 * At least one other writer is waiting, clear
			 * it from the count as we pass it ownership.
			 */
			next.u.count = prev.u.count + 1;
			next.u.rdpend = prev.u.rdpend;
		} else {
			/*
			 * No writer waits for this lock, enable all
			 * readers or a single writer to take it next.
			 */
			assert(prev.u.count == 0);
			next.u.count = __EVL_RWLOCK_WRBIAS;
			next.u.rdpend = 0;
		}

		oldval = atomic_cmpxchg_weak(&rwlock->u.lock, prev.value, next.value);
		if (oldval == prev.value)
			break;
	}

	/* Priority to writers in getting a released lock. */
	if (prev.u.count < 0)
		/* Wake up one writer. */
		ret = evl_post_flags(&rwlock->event, __EVL_RWLOCK_WR);
	else if (prev.u.rdpend)
		/* Wake up all readers. */
		ret = evl_broadcast_flags(&rwlock->event, __EVL_RWLOCK_RD);

	return ret;
}
