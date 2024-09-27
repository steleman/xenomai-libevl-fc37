/*
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sched.h>
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

static struct evl_mutex lock;

static struct evl_sem start;

static struct evl_sem wait;

static bool check_priority(int tfd, int prio)
{
	struct evl_thread_state statebuf;
	int ret;

	__Tcall_assert(ret, evl_get_state(tfd, &statebuf));

	return statebuf.eattrs.sched_policy == SCHED_FIFO &&
		statebuf.eattrs.sched_priority == prio;
}

static void pin_to_cpu0(void)
{
	cpu_set_t affinity;
	int ret;

	CPU_ZERO(&affinity);
	CPU_SET(0, &affinity);
	__Tcall_assert(ret, sched_setaffinity(0, sizeof(affinity), &affinity));
}

static void *contender(void *arg)
{
	int tfd, ret;

	pin_to_cpu0();

	__Tcall_assert(tfd, evl_attach_self("monitor-pp-contender:%d",
						getpid()));
	__Tcall_assert(ret, evl_put_sem(&start));
	__Tcall_assert(ret, evl_get_sem(&wait));
	__Tcall_assert(ret, evl_lock_mutex(&lock));
	__Tcall_assert(ret, evl_unlock_mutex(&lock));

	return NULL;
}

int main(int argc, char *argv[])
{
	struct sched_param param;
	int tfd, gfd, sfd, ret;
	pthread_t tid;
	char *name;

	pin_to_cpu0();

	param.sched_priority = LOW_PRIO;
	__Texpr_assert(pthread_setschedparam(pthread_self(),
				SCHED_FIFO, &param) == 0);

	/* EVL inherits the inband scheduling params upon attachment. */
	__Tcall_assert(tfd, evl_attach_self("monitor-pp-lazy:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(gfd, evl_create_mutex(&lock, EVL_CLOCK_MONOTONIC,
					HIGH_PRIO, EVL_MUTEX_NORMAL, name));

	name = get_unique_name(EVL_MONITOR_DEV, 1);
	__Tcall_assert(sfd, evl_new_sem(&start, name));

	name = get_unique_name(EVL_MONITOR_DEV, 2);
	__Tcall_assert(sfd, evl_new_sem(&wait, name));

	new_thread(&tid, SCHED_FIFO, HIGH_PRIO, contender, NULL);
	__Tcall_assert(ret, evl_get_sem(&start));
	__Tcall_assert(ret, evl_lock_mutex(&lock));
	__Tcall_assert(ret, evl_put_sem(&wait));
	/*
	 * We should have been raised to the ceiling priority upon
	 * preemption by the contender due to evl_put_sem().
	 */
	__Texpr_assert(check_priority(tfd, HIGH_PRIO));
	__Tcall_assert(ret, evl_unlock_mutex(&lock));
	__Texpr_assert(pthread_join(tid, NULL) == 0);

	evl_close_mutex(&lock);
	evl_close_sem(&start);
	evl_close_sem(&wait);

	return 0;
}
