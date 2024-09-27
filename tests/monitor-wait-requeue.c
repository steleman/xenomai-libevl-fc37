/*
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <evl/atomic.h>
#include <evl/sched.h>
#include <evl/sched-evl.h>
#include <evl/thread.h>
#include <evl/thread-evl.h>
#include <evl/mutex.h>
#include <evl/mutex-evl.h>
#include <evl/sem.h>
#include "helpers.h"

#define LOW_PRIO	1
#define MEDIUM_PRIO	2
#define HIGH_PRIO	3

static atomic_t owner_id;

static struct evl_mutex lock;

static struct evl_sem barrier;

static struct test_context {
	int tfd;
} c[2];

static void *waiter(void *arg)
{
	struct test_context *p = arg;
	int prev, me, ret;

	me = (p - &c[0]) + 1;	/* 1 or 2, depending on the waiter position in c[]. */
	__Tcall_assert(p->tfd, evl_attach_self("monitor-requeued-waiter:%d/%d", getpid(), me));
	__Tcall_assert(ret, evl_put_sem(&barrier));

	__Tcall_assert(ret, evl_lock_mutex(&lock));

	/* Succeeds on first swap only. */
	prev = 0;
	atomic_compare_exchange_strong_explicit(
		&owner_id, &prev, me,
		__ATOMIC_RELEASE, __ATOMIC_ACQUIRE);

	__Tcall_assert(ret, evl_unlock_mutex(&lock));

	return NULL;
}

int main(int argc, char *argv[])
{
	struct evl_sched_attrs attrs;
	int tfd, gfd, sfd, ret, val;
	pthread_t waiters[2];
	void *status = NULL;
	char *name;

	/*
	 * EVL inherits the inband scheduling params upon attachment,
	 * so the core should apply SCHED_WEAK to us.
	 */
	__Tcall_assert(tfd, evl_attach_self("monitor-wait-requeued:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(gfd, evl_new_mutex(&lock, name));

	name = get_unique_name(EVL_MONITOR_DEV, 1);
	__Tcall_assert(sfd, evl_new_sem(&barrier, name));

	/*
	 * Disable WOLI in case CONFIG_EVL_DEBUG_WOLI is set, as we
	 * are about to sleep while holding a mutex.
	 */
	__Tcall_assert(ret, evl_clear_thread_mode(tfd, EVL_T_WOLI, NULL));
	__Tcall_assert(ret, evl_lock_mutex(&lock));

	/* Start the waiters racing on the same lock. */
	new_thread(&waiters[0], SCHED_FIFO, LOW_PRIO, waiter, &c[0]);
	new_thread(&waiters[1], SCHED_FIFO, MEDIUM_PRIO, waiter, &c[1]);

	/* Wait for the two waiters to execute. */
	__Tcall_assert(ret, evl_get_sem(&barrier));
	__Tcall_assert(ret, evl_get_sem(&barrier));

	/*
	 * Both waiters are sleeping on lock now, bump priority for #0
	 * to HIGH.
	 */
	attrs.sched_policy = SCHED_FIFO;
	attrs.sched_priority = HIGH_PRIO;
	__Tcall_assert(ret, evl_set_schedattr(c[0].tfd, &attrs));

	/* Release the lock, #0 should grab it first. */
	__Tcall_assert(ret, evl_unlock_mutex(&lock));

	/* Collect the results, check consistency. */
	__Texpr_assert(pthread_join(waiters[0], &status) == 0);
	__Texpr_assert(status == NULL);
	__Texpr_assert(pthread_join(waiters[1], &status) == 0);
	__Texpr_assert(status == NULL);
	val = atomic_load_explicit(&owner_id, __ATOMIC_ACQUIRE);
	__Texpr_assert(val == 1); /* waiter #0 should have it. */

	__Tcall_assert(ret, evl_close_sem(&barrier));
	__Tcall_assert(ret, evl_close_mutex(&lock));

	return 0;
}
