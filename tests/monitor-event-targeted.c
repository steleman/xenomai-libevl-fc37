/*
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <evl/atomic.h>
#include <evl/thread.h>
#include <evl/thread-evl.h>
#include <evl/mutex.h>
#include <evl/mutex-evl.h>
#include <evl/event.h>
#include <evl/sem.h>
#include <evl/clock.h>
#include <evl/clock-evl.h>
#include "helpers.h"

#define NR_TEST_LOOPS   1500
#define NR_RECEIVERS    3

#define LOW_PRIO	1
#define HIGH_PRIO	2

static struct evl_mutex lock;

static struct evl_event event;

static bool done;

static int last_receiverfd = -1;

static struct test_context {
	struct evl_sem sync;
	int tfd;
} c[NR_RECEIVERS];

static void *receiver(void *arg)
{
	struct test_context *p = arg;
	int ret, tfd, pos = p - c;

	__Tcall_assert(tfd, evl_attach_self("monitor-event-target:%d/%d",
						getpid(), pos));
	p->tfd = tfd;
	__Tcall_assert(ret, evl_put_sem(&p->sync));
	__Tcall_assert(ret, evl_lock_mutex(&lock));

	do {
		__Tcall_assert(ret, evl_wait_event(&event, &lock));
		last_receiverfd = tfd;
	} while (!done);

	__Tcall_assert(ret, evl_unlock_mutex(&lock));

	return NULL;
}

int main(int argc, char *argv[])
{
	int tfd, gfd, evfd, sfd, n, ret, loops;
	pthread_t receivers[NR_RECEIVERS];
	struct sched_param param;
	char *name;

	param.sched_priority = LOW_PRIO;
	__Texpr_assert(pthread_setschedparam(pthread_self(),
				SCHED_FIFO, &param) == 0);

	/* EVL inherits the inband scheduling params upon attachment. */
	__Tcall_assert(tfd, evl_attach_self("monitor-event-sender:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(gfd, evl_new_mutex(&lock, name));

	name = get_unique_name(EVL_MONITOR_DEV, 1);
	__Tcall_assert(evfd, evl_new_event(&event, name));

	for (n = 0; n < NR_RECEIVERS; n++) {
		name = get_unique_name(EVL_MONITOR_DEV, 2 + n);
		__Tcall_assert(sfd, evl_new_sem(&c[n].sync, name));
		new_thread(receivers + n, SCHED_FIFO, HIGH_PRIO, receiver, c + n);
		__Tcall_assert(ret, evl_get_sem(&c[n].sync));
	}

	for (loops = 0; loops < NR_TEST_LOOPS; loops++) {
		for (n = 0; n < NR_RECEIVERS; n++) {
			__Tcall_assert(ret, evl_usleep(1000));
			__Tcall_assert(ret, evl_lock_mutex(&lock));
			__Tcall_assert(ret, evl_signal_thread(&event, c[n].tfd));
			__Tcall_assert(ret, evl_unlock_mutex(&lock));
			__Tcall_assert(ret, evl_lock_mutex(&lock));
			__Texpr_assert(last_receiverfd == c[n].tfd);
			__Tcall_assert(ret, evl_unlock_mutex(&lock));
		}
	}

	__Tcall_assert(ret, evl_lock_mutex(&lock));
	done = true;
	__Tcall_assert(ret, evl_broadcast_event(&event));
	__Tcall_assert(ret, evl_unlock_mutex(&lock));

	for (n = 0; n < NR_RECEIVERS; n++) {
		__Texpr_assert(pthread_join(receivers[n], NULL) == 0);
		__Tcall_assert(ret, evl_close_sem(&c[n].sync));
	}

	__Tcall_assert(ret, evl_close_event(&event));
	__Tcall_assert(ret, evl_close_mutex(&lock));

	return 0;
}
