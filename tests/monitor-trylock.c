/*
 * SPDX-License-Identifier: MIT
 *
 * Exercise fallible locking (trylock) on contention.
 *
 * Based on a test code written by Giulio Moro <giulio@bela.io>.
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
#include "helpers.h"

/*
 * Up to four contending threads, fanned out over the available CPUs.
 */
#define NR_CONTENDERS   4

#define NR_ITERATIONS   20000

#define LOW_PRIO	1
#define HIGH_PRIO	2

static struct evl_mutex lock;

static bool do_stop;

static void *contender_thread(void *arg)
{
	int cpu_hint = (int)(long)arg, cpu, tfd, ret;
	cpu_set_t affinity;
	bool stopped;

	cpu = pick_test_cpu(cpu_hint, false, NULL);
	CPU_ZERO(&affinity);
	CPU_SET(cpu, &affinity);
	__Tcall_assert(ret, sched_setaffinity(0, sizeof(affinity), &affinity));

	__Tcall_assert(tfd, evl_attach_self("monitor-trylock-%c:%d",
					'A' + cpu_hint, getpid()));
	do {
		__Tcall_assert(ret, evl_lock_mutex(&lock));
		stopped = do_stop;
		__Tcall_assert(ret, evl_unlock_mutex(&lock));
		evl_usleep(100);
	} while (!stopped);

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t contenders[NR_CONTENDERS];
	struct sched_param param;
	int tfd, mfd, ret, n;
	char *name;

	param.sched_priority = HIGH_PRIO;
	__Texpr_assert(pthread_setschedparam(pthread_self(),
				SCHED_FIFO, &param) == 0);

	/* EVL inherits the inband scheduling params upon attachment. */
	__Tcall_assert(tfd, evl_attach_self("monitor-try-main:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(mfd, evl_new_mutex(&lock, name));

	for (n = 0; n < NR_CONTENDERS; n++)
		new_thread(contenders + n, SCHED_FIFO, LOW_PRIO,
			contender_thread, (void *)(long)n);

	for (n = 0; n < NR_ITERATIONS; n++) {
		ret = evl_trylock_mutex(&lock);
		switch (ret) {
		case -EBUSY:
			break;
		case 0:
			__Tcall_assert(ret, evl_unlock_mutex(&lock));
			break;
		default:
			__Texpr_assert(0);
		}
		evl_usleep(100);
	}

	__Tcall_assert(ret, evl_lock_mutex(&lock));
	do_stop = true;
	__Tcall_assert(ret, evl_unlock_mutex(&lock));

	for (n = 0; n < NR_CONTENDERS; n++)
		__Texpr_assert(pthread_join(contenders[n], NULL) == 0);

	evl_close_mutex(&lock);

	return 0;
}
