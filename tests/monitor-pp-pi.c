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
#define MEDIUM_PRIO	2
#define HIGH_PRIO	3

struct test_context {
	struct evl_mutex lock;
	struct evl_sem start;
	struct evl_sem sem;
};

static void *pi_contend_timeout(void *arg)
{
	struct test_context *p = arg;
	struct timespec now, timeout;
	int ret, tfd;

	__Tcall_assert(tfd, evl_attach_self("monitor-pi-contend:%d", getpid()));
	__Tcall_assert(ret, evl_get_sem(&p->start));

	evl_read_clock(EVL_CLOCK_MONOTONIC, &now);
	timespec_add_ns(&timeout, &now, 10000000); /* 10ms */

	__Tcall_assert(ret, evl_put_sem(&p->sem));

	if (__Fcall(ret, evl_timedlock_mutex(&p->lock, &timeout)) &&
		__Texpr(ret == -ETIMEDOUT))
		return (void *)1;

	return NULL;
}

static bool check_priority(int tfd, int prio)
{
	struct evl_thread_state statebuf;
	int ret;

	__Tcall_assert(ret, evl_get_state(tfd, &statebuf));

	return statebuf.eattrs.sched_policy == SCHED_FIFO &&
		statebuf.eattrs.sched_priority == prio;
}

int main(int argc, char *argv[])
{
	struct sched_param param;
	struct evl_mutex lock_pp;
	int tfd, gfd, sfd, ret;
	struct test_context c;
	pthread_t contender;
	void *status = NULL;
	char *name;

	param.sched_priority = LOW_PRIO;
	__Texpr_assert(pthread_setschedparam(pthread_self(),
				SCHED_FIFO, &param) == 0);

	/* EVL inherits the inband scheduling params upon attachment. */
	__Tcall_assert(tfd, evl_attach_self("monitor-pp-pi:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(gfd, evl_new_mutex(&c.lock, name));

	name = get_unique_name(EVL_MONITOR_DEV, 1);
	__Tcall_assert(gfd, evl_create_mutex(&lock_pp, EVL_CLOCK_MONOTONIC,
					MEDIUM_PRIO, EVL_MUTEX_NORMAL, name));

	name = get_unique_name(EVL_MONITOR_DEV, 2);
	__Tcall_assert(sfd, evl_new_sem(&c.sem, name));

	name = get_unique_name(EVL_MONITOR_DEV, 3);
	__Tcall_assert(sfd, evl_new_sem(&c.start, name));

	new_thread(&contender, SCHED_FIFO, HIGH_PRIO,
			pi_contend_timeout, &c);

	/*
	 * Disable WOLI in case CONFIG_EVL_DEBUG_WOLI is set, as we
	 * are about to sleep while holding a mutex.
	 */
	__Tcall_assert(ret, evl_clear_thread_mode(tfd, EVL_T_WOLI, NULL));
	__Tcall_assert(ret, evl_lock_mutex(&lock_pp));
	__Tcall_assert(ret, evl_usleep(5000)); /* Commit PP boost. */
	__Texpr_assert(check_priority(tfd, MEDIUM_PRIO));
	__Tcall_assert(ret, evl_lock_mutex(&c.lock));
	__Tcall_assert(ret, evl_put_sem(&c.start));
	__Tcall_assert(ret, evl_get_sem(&c.sem));
	__Texpr_assert(check_priority(tfd, HIGH_PRIO));
	__Texpr_assert(pthread_join(contender, &status) == 0);
	__Tcall_assert(ret, evl_unlock_mutex(&c.lock));
	__Texpr_assert(check_priority(tfd, MEDIUM_PRIO));
	__Fexpr_assert(status == NULL);
	__Tcall_assert(ret, evl_unlock_mutex(&lock_pp));
	__Texpr_assert(check_priority(tfd, LOW_PRIO));

	evl_close_sem(&c.start);
	evl_close_sem(&c.sem);
	evl_close_mutex(&c.lock);
	evl_close_mutex(&lock_pp);

	return 0;
}
