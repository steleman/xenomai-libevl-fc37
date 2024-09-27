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
#include <evl/sched.h>
#include <evl/sched-evl.h>
#include <evl/sem.h>
#include <evl/flags.h>
#include "helpers.h"

#define NR_LOOPS	15000

#define LOW_PRIO	1
#define MEDIUM_PRIO	2
#define HIGH_PRIO	3

static struct evl_mutex lock_1;
static struct evl_mutex lock_2;
static struct evl_sem init;
static struct evl_flags start;
static struct evl_sem stop;

static bool check_priority(int tfd, int prio)
{
	struct evl_thread_state statebuf;
	int ret;

	__Tcall_assert(ret, evl_get_state(tfd, &statebuf));

	return statebuf.eattrs.sched_policy == SCHED_FIFO &&
		statebuf.eattrs.sched_priority == prio;
}

static void pin_to_cpu(int cpu)
{
	cpu_set_t affinity;
	int ret;

	CPU_ZERO(&affinity);
	CPU_SET(cpu, &affinity);
	__Tcall_assert(ret, sched_setaffinity(0, sizeof(affinity), &affinity));
}

static void *deboost_contend(void *arg)
{
	int ret, tfd, cpu = (int)(long)arg;
	struct timespec now, timeout;
	int flags;

	__Tcall_assert(tfd, evl_attach_self("monitor-deboost-contend:%d.%d",
						getpid(), cpu));
	__Tcall_assert(ret, evl_put_sem(&init));
	__Tcall_assert(ret, evl_wait_flags(&start, &flags));

	/*
	 * Hopefully CPU0-2 are available, otherwise the test would
	 * not be as stressful as expected.
	 */
	cpu = pick_test_cpu(cpu, false, NULL);
	pin_to_cpu(cpu);

	do {
		evl_read_clock(EVL_CLOCK_MONOTONIC, &now);
		timespec_add_ns(&timeout, &now, 100000 + (cpu - 1) * 50000); /* 100/150us */
		__Fcall_assert(ret, evl_timedlock_mutex(&lock_1, &timeout));
		__Texpr_assert(ret == -ETIMEDOUT);
		ret = evl_tryget_sem(&stop);
	} while (ret == -EAGAIN);

	__Texpr_assert(ret == 0);

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t contender1, contender2;
	int tfd, gfd, sfd, ret, n;
	struct sched_param param;
	void *status = NULL;
	char *name;

	pin_to_cpu(0);		/* to CPU0 */

	param.sched_priority = LOW_PRIO;
	__Texpr_assert(pthread_setschedparam(pthread_self(),
				SCHED_FIFO, &param) == 0);

	/* EVL inherits the inband scheduling params upon attachment. */
	__Tcall_assert(tfd, evl_attach_self("monitor-deboost-stress:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(gfd, evl_new_mutex(&lock_1, name));

	name = get_unique_name(EVL_MONITOR_DEV, 1);
	__Tcall_assert(gfd, evl_new_mutex(&lock_2, name));

	name = get_unique_name(EVL_MONITOR_DEV, 2);
	__Tcall_assert(sfd, evl_new_flags(&start, name));

	name = get_unique_name(EVL_MONITOR_DEV, 3);
	__Tcall_assert(sfd, evl_new_sem(&stop, name));

	name = get_unique_name(EVL_MONITOR_DEV, 4);
	__Tcall_assert(sfd, evl_new_sem(&init, name));

	new_thread(&contender2, SCHED_FIFO, MEDIUM_PRIO,
		deboost_contend, (void *)(long)1); /* to CPU1 */

	new_thread(&contender1, SCHED_FIFO, HIGH_PRIO,
		deboost_contend, (void *)(long)2); /* to CPU2 */

	__Tcall_assert(ret, evl_lock_mutex(&lock_1));
	__Tcall_assert(ret, evl_get_sem(&init));
	__Tcall_assert(ret, evl_get_sem(&init));
	__Tcall_assert(ret, evl_post_flags(&start, 1));
	__Tcall_assert(ret, evl_post_flags(&start, 1));

	for (n = 0; n < NR_LOOPS; n++) {
		__Tcall_assert(ret, evl_lock_mutex(&lock_2));
		__Tcall_assert(ret, evl_unlock_mutex(&lock_2));
		__Tcall_assert(ret, evl_usleep(300));
	}

	__Tcall_assert(ret, evl_put_sem(&stop));
	__Tcall_assert(ret, evl_put_sem(&stop));
	__Texpr_assert(pthread_join(contender1, &status) == 0);
	__Texpr_assert(pthread_join(contender2, &status) == 0);
	__Tcall_assert(ret, evl_unlock_mutex(&lock_1));

	/* Make sure we switched back to our normal priority. */
	__Texpr_assert(check_priority(tfd, LOW_PRIO));

	evl_close_flags(&start);
	evl_close_sem(&stop);
	evl_close_mutex(&lock_1);
	evl_close_mutex(&lock_2);

	return 0;
}
