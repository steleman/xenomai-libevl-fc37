/*
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#include <sched.h>
#include <errno.h>
#include <evl/atomic.h>
#include <evl/thread.h>
#include <evl/thread-evl.h>
#include <evl/sched.h>
#include <evl/sched-evl.h>
#include <evl/event.h>
#include <evl/mutex.h>
#include <evl/mutex-evl.h>
#include <evl/clock.h>
#include <evl/clock-evl.h>
#include <evl/sem.h>
#include "helpers.h"

#define LOW_PRIO	1
#define MEDIUM_PRIO	2
#define HIGH_PRIO	3

#define WAIT_TIMEOUT	100000000	/* 100ms */
#define BUSY_TIMEOUT	(WAIT_TIMEOUT + 1000000)

struct test_context {
	struct evl_mutex lock;
	struct evl_event event;
	struct evl_sem start;
};

static void pin_to_cpu0(void)
{
	cpu_set_t affinity;
	int ret;

	CPU_ZERO(&affinity);
	CPU_SET(0, &affinity);
	__Tcall_assert(ret, sched_setaffinity(0, sizeof(affinity), &affinity));
}

static void *event_waiter(void *arg)
{
	struct test_context *p = arg;
	struct timespec now, timeout;
	int tfd, ret;

	pin_to_cpu0();

	__Tcall_assert(tfd, evl_attach_self("monitor-event-waiter:%d", getpid()));

	__Tcall_assert(ret, evl_lock_mutex(&p->lock));

	__Tcall_assert(ret, evl_put_sem(&p->start));

	evl_read_clock(EVL_CLOCK_MONOTONIC, &now);
	timespec_add_ns(&timeout, &now, WAIT_TIMEOUT);

	__Fcall_assert(ret, evl_timedwait_event(&p->event, &p->lock, &timeout));
	__Texpr(ret == -ETIMEDOUT);

	__Tcall_assert(ret, evl_unlock_mutex(&p->lock));

	return NULL;
}

int main(int argc, char *argv[])
{
	int tfd, mfd, evfd, sfd, ret;
	struct evl_sched_attrs attrs;
	struct timespec start, now;
	struct sched_param param;
	struct test_context c;
	void *status = NULL;
	pthread_t waiter;
	char *name;

	/* All threads must run on the same CPU. */
	pin_to_cpu0();

	param.sched_priority = LOW_PRIO;
	__Texpr_assert(pthread_setschedparam(pthread_self(),
				SCHED_FIFO, &param) == 0);

	/* EVL inherits the inband scheduling params upon attachment. */
	__Tcall_assert(tfd, evl_attach_self("monitor-event-untrack:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(sfd, evl_new_sem(&c.start, name));

	name = get_unique_name(EVL_MONITOR_DEV, 1);
	__Tcall_assert(evfd, evl_new_event(&c.event, name));

	name = get_unique_name(EVL_MONITOR_DEV, 2);
	__Tcall_assert(mfd, evl_new_mutex(&c.lock, name));

	new_thread(&waiter, SCHED_FIFO, MEDIUM_PRIO, event_waiter, &c);

	__Tcall_assert(ret, evl_get_sem(&c.start));

	/*
	 * Raise our scheduling priority above the waiter's so that it
	 * won't preempt us when signaling the event. We must complete
	 * this within WAIT_TIMEOUT ns.
	 */
	attrs.sched_policy = SCHED_FIFO;
	attrs.sched_priority = HIGH_PRIO;
	__Tcall_assert(ret, evl_set_schedattr(tfd, &attrs));

	/*
	 * Now run a loop consuming CPU at high priority in order to
	 * cause a timeout condition for the waiter, without allowing
	 * it to preempt us on the current CPU.
	 */
	evl_read_clock(EVL_CLOCK_MONOTONIC, &start);
	for (;;) {
		evl_read_clock(EVL_CLOCK_MONOTONIC, &now);
		if (timespec_sub_ns(&now, &start) > BUSY_TIMEOUT)
			break;
	}

	/*
	 * We know the timeout condition is raised by now, but the
	 * waiter could not consume it just yet. Post the event.
	 */
	__Tcall_assert(ret, evl_lock_mutex(&c.lock));
	__Tcall_assert(ret, evl_signal_event(&c.event));
	__Tcall_assert(ret, evl_unlock_mutex(&c.lock));

	__Texpr_assert(pthread_join(waiter, &status) == 0);
	__Texpr_assert(status == NULL);

	__Tcall_assert(ret, evl_close_sem(&c.start));
	__Tcall_assert(ret, evl_close_event(&c.event));
	__Tcall_assert(ret, evl_close_mutex(&c.lock));

	return 0;
}
