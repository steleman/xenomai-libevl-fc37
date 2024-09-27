/*
 * SPDX-License-Identifier: MIT
 *
 * Stress the PI chain walk with multiple threads triggering
 * boost/deboost operations concurrenly on shared locks. The logic of
 * this code was originally designed by Russell Johnson.
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
#include <evl/clock.h>
#include <evl/clock-evl.h>
#include "helpers.h"

#define A_PRIO  4
#define B_PRIO  3
#define C_PRIO  2
#define D_PRIO  1

static atomic_t done;

struct test_context {
	struct evl_mutex lock_1;
	struct evl_mutex lock_2;
	struct evl_mutex lock_3;
	struct evl_event event_1;
	struct evl_event event_2;
	struct evl_event event_3;
};

static bool check_priority(int tfd, int prio)
{
	struct evl_thread_state statebuf;
	int ret;

	__Tcall_assert(ret, evl_get_state(tfd, &statebuf));

	return statebuf.eattrs.sched_policy == SCHED_FIFO &&
		statebuf.eattrs.sched_priority == prio;
}

static void *thread_a(void *arg)
{
	struct test_context *p = arg;
	int ret, tfd;

	__Tcall_assert(tfd, evl_attach_self("monitor-pi-stress-A:%d", getpid()));

	do {
		__Tcall_assert(ret, evl_usleep(100000));
		__Tcall_assert(ret, evl_lock_mutex(&p->lock_1));
		__Tcall_assert(ret, evl_signal_event(&p->event_1));
		__Tcall_assert(ret, evl_unlock_mutex(&p->lock_1));
	} while (!atomic_load(&done));

	__Texpr_assert(check_priority(tfd, A_PRIO));

	return NULL;
}

static void *thread_b(void *arg)
{
	struct test_context *p = arg;
	struct timespec now, timeout;
	int ret, tfd, n;

	__Tcall_assert(tfd, evl_attach_self("monitor-pi-stress-B:%d", getpid()));

	for (;;) {
		evl_read_clock(EVL_CLOCK_MONOTONIC, &now);
		timespec_add_ns(&timeout, &now, 400000000); /* 400ms */
		__Tcall_assert(ret, evl_lock_mutex(&p->lock_1));
		ret = evl_timedwait_event(&p->event_1, &p->lock_1, &timeout);
		__Tcall_assert(ret, evl_unlock_mutex(&p->lock_1));
		if (atomic_load(&done))
			break;
		__Texpr_assert(ret == 0);
		for (n = 0; n < 50; n++) {
			__Tcall_assert(ret, evl_lock_mutex(&p->lock_2));
			__Tcall_assert(ret, evl_signal_event(&p->event_2));
			__Tcall_assert(ret, evl_unlock_mutex(&p->lock_2));
		}
	}

	__Texpr_assert(check_priority(tfd, B_PRIO));

	return NULL;
}

static void *thread_c(void *arg)
{
	struct test_context *p = arg;
	struct timespec now, timeout;
	int ret, tfd;

	__Tcall_assert(tfd, evl_attach_self("monitor-pi-stress-C:%d", getpid()));

	for (;;) {
		evl_read_clock(EVL_CLOCK_MONOTONIC, &now);
		timespec_add_ns(&timeout, &now, 400000000); /* 400ms */
		__Tcall_assert(ret, evl_lock_mutex(&p->lock_2));
		ret = evl_timedwait_event(&p->event_2, &p->lock_2, &timeout);
		__Tcall_assert(ret, evl_unlock_mutex(&p->lock_2));
		if (atomic_load(&done))
			break;
		__Texpr_assert(ret == 0);
		__Tcall_assert(ret, evl_lock_mutex(&p->lock_3));
		__Tcall_assert(ret, evl_signal_event(&p->event_3));
		__Tcall_assert(ret, evl_unlock_mutex(&p->lock_3));
	}

	__Texpr_assert(check_priority(tfd, C_PRIO));

	return NULL;
}

static void *thread_d(void *arg)
{
	struct test_context *p = arg;
	struct timespec now, timeout;
	int ret, tfd, n;

	__Tcall_assert(tfd, evl_attach_self("monitor-pi-stress-D:%d", getpid()));

	for (;;) {
		__Tcall_assert(ret, evl_lock_mutex(&p->lock_3));

		for (n = 0; n < 50; n++) {
			evl_read_clock(EVL_CLOCK_MONOTONIC, &now);
			timespec_add_ns(&timeout, &now, 400000000); /* 400ms */
			ret = evl_timedwait_event(&p->event_3, &p->lock_3, &timeout);
			if (atomic_load(&done)) {
				__Tcall_assert(ret, evl_unlock_mutex(&p->lock_3));
				return NULL;
			}
			__Texpr_assert(ret == 0);
		}

		__Tcall_assert(ret, evl_unlock_mutex(&p->lock_3));
	}

	__Texpr_assert(check_priority(tfd, D_PRIO));

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t t_a, t_b, t_c, t_d;
	struct test_context c;
	int tfd, gfd, evfd;
	char *name;

	__Tcall_assert(tfd, evl_attach_self("monitor-pi-stress:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(gfd, evl_new_mutex(&c.lock_1, name)); /* PI-implicit form. */

	name = get_unique_name(EVL_MONITOR_DEV, 1);
	__Tcall_assert(gfd, evl_new_mutex(&c.lock_2, name));

	name = get_unique_name(EVL_MONITOR_DEV, 2);
	__Tcall_assert(gfd, evl_new_mutex(&c.lock_3, name));

	name = get_unique_name(EVL_MONITOR_DEV, 3);
	__Tcall_assert(evfd, evl_new_event(&c.event_1, name));

	name = get_unique_name(EVL_MONITOR_DEV, 4);
	__Tcall_assert(evfd, evl_new_event(&c.event_2, name));

	name = get_unique_name(EVL_MONITOR_DEV, 5);
	__Tcall_assert(evfd, evl_new_event(&c.event_3, name));

	new_thread(&t_a, SCHED_FIFO, A_PRIO, thread_a, &c);
	new_thread(&t_b, SCHED_FIFO, B_PRIO, thread_b, &c);
	new_thread(&t_c, SCHED_FIFO, C_PRIO, thread_c, &c);
	new_thread(&t_d, SCHED_FIFO, D_PRIO, thread_d, &c);

	sleep(7); /* This should be enough to trigger issues if any. */
	atomic_store(&done, 1);

	__Texpr_assert(pthread_join(t_a, NULL) == 0);
	__Texpr_assert(pthread_join(t_b, NULL) == 0);
	__Texpr_assert(pthread_join(t_c, NULL) == 0);
	__Texpr_assert(pthread_join(t_d, NULL) == 0);

	/* Let the core flush all the resources. */

	return 0;
}
