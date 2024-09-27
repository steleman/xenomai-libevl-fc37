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
#include <evl/thread.h>
#include <evl/thread-evl.h>
#include <evl/mutex.h>
#include <evl/mutex-evl.h>
#include <evl/clock.h>
#include <evl/clock-evl.h>
#include <evl/sem.h>
#include "helpers.h"

#define LOW_PRIO	1
#define HIGH_PRIO	2

struct test_context {
	struct evl_mutex lock;
	struct evl_sem start;
	struct evl_sem sem;
};

static bool check_priority(int tfd, int prio)
{
	struct evl_thread_state statebuf;
	int ret;

	__Tcall_assert(ret, evl_get_state(tfd, &statebuf));

	return statebuf.eattrs.sched_policy == SCHED_FIFO &&
		statebuf.eattrs.sched_priority == prio;
}

static void *pi_waiter(void *arg)
{
	struct test_context *p = arg;
	struct timespec now, timeout;
	int ret, tfd;

	__Tcall_assert(tfd, evl_attach_self("monitor-pi-waiter:%d", getpid()));
	__Tcall_assert(ret, evl_get_sem(&p->start));

	evl_read_clock(EVL_CLOCK_MONOTONIC, &now);
	timespec_add_ns(&timeout, &now, 200000000); /* 200ms */

	__Tcall_assert(ret, evl_put_sem(&p->sem));

	if (__Fcall(ret, evl_timedlock_mutex(&p->lock, &timeout)) &&
		__Texpr(ret == -ETIMEDOUT)) {
		__Tcall_assert(ret, evl_put_sem(&p->sem));
		return (void *)1;
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	struct sched_param param;
	int tfd, gfd, sfd, ret;
	struct test_context c;
	void *status = NULL;
	pthread_t waiter;
	char *name;

	param.sched_priority = LOW_PRIO;
	__Texpr_assert(pthread_setschedparam(pthread_self(),
				SCHED_FIFO, &param) == 0);

	/* EVL inherits the inband scheduling params upon attachment. */
	__Tcall_assert(tfd, evl_attach_self("monitor-pi-deboost:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(gfd, evl_new_mutex(&c.lock, name));

	name = get_unique_name(EVL_MONITOR_DEV, 1);
	__Tcall_assert(sfd, evl_new_sem(&c.sem, name));

	name = get_unique_name(EVL_MONITOR_DEV, 2);
	__Tcall_assert(sfd, evl_new_sem(&c.start, name));

	new_thread(&waiter, SCHED_FIFO, HIGH_PRIO, pi_waiter, &c);

	/*
	 * Disable WOLI in case CONFIG_EVL_DEBUG_WOLI is set, as we
	 * are about to sleep while holding a mutex.
	 */
	__Tcall_assert(ret, evl_clear_thread_mode(tfd, EVL_T_WOLI, NULL));
	__Tcall_assert(ret, evl_lock_mutex(&c.lock));
	__Tcall_assert(ret, evl_put_sem(&c.start));
	__Tcall_assert(ret, evl_get_sem(&c.sem));

	/*
	 * We should have been boosted by now, since we hold c.lock
	 * which the high priority waiter wants as well.
	 */
	__Texpr_assert(check_priority(tfd, HIGH_PRIO));
	__Tcall_assert(ret, evl_get_sem(&c.sem));

	/*
	 * As the waiter stopped waiting for the lock on timeout, we
	 * should have been deboosted by now.
	 */
	__Texpr_assert(check_priority(tfd, LOW_PRIO));

	__Texpr_assert(pthread_join(waiter, &status) == 0);
	__Tcall_assert(ret, evl_unlock_mutex(&c.lock));
	__Fexpr_assert(status == NULL);

	evl_close_sem(&c.start);
	evl_close_sem(&c.sem);
	evl_close_mutex(&c.lock);

	return 0;
}
