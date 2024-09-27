/*
 * SPDX-License-Identifier: MIT
 *
 * Detect ABBA deadlock pattern walking the PI chain.
 */

#include <sys/types.h>
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
#include <evl/sem.h>
#include "helpers.h"

#define LOW_PRIO	1
#define MEDIUM_PRIO	2
#define HIGH_PRIO	3

struct test_context {
	struct evl_mutex lock_a;
	struct evl_mutex lock_b;
	struct evl_mutex lock_c;
	struct evl_sem start;
	struct evl_sem sync;
};

static bool check_priority(int tfd, int prio)
{
	struct evl_thread_state statebuf;
	int ret;

	__Tcall_assert(ret, evl_get_state(tfd, &statebuf));

	return statebuf.eattrs.sched_policy == SCHED_FIFO &&
		statebuf.eattrs.sched_priority == prio;
}

static void *thread_c(void *arg)
{
	struct test_context *p = arg;
	int ret, tfd;

	__Tcall_assert(tfd, evl_attach_self("monitor-pi-dlk-C:%d", getpid()));
	/*
	 * Disable WOLI in case CONFIG_EVL_DEBUG_WOLI is set, as we
	 * are about to sleep while holding a mutex.
	 */
	__Tcall_assert(ret, evl_clear_thread_mode(tfd, EVL_T_WOLI, NULL));
	__Tcall_assert(ret, evl_lock_mutex(&p->lock_c));
	__Tcall_assert(ret, evl_put_sem(&p->sync));

	/* Wait for start call. */
	__Tcall_assert(ret, evl_get_sem(&p->start));
	/* We should have inherited thread_a effective priority. */
	__Texpr_assert(check_priority(tfd, HIGH_PRIO));

	__Fcall_assert(ret, evl_lock_mutex(&p->lock_a)); /* ABBA deadlock */
	__Texpr_assert(ret == -EDEADLK);
	__Tcall_assert(ret, evl_unlock_mutex(&p->lock_c));
	/* We should have been deboosted to our initial priority. */
	__Texpr_assert(check_priority(tfd, LOW_PRIO));

	return NULL;
}

static void *thread_b(void *arg)
{
	struct test_context *p = arg;
	int ret, tfd;

	__Tcall_assert(tfd, evl_attach_self("monitor-pi-dlk-B:%d", getpid()));
	__Tcall_assert(ret, evl_clear_thread_mode(tfd, EVL_T_WOLI, NULL));
	__Tcall_assert(ret, evl_lock_mutex(&p->lock_b));
	__Tcall_assert(ret, evl_put_sem(&p->sync));

	/* Wait for start call. */
	__Tcall_assert(ret, evl_get_sem(&p->start));
	/* We should have inherited thread_a effective priority. */
	__Texpr_assert(check_priority(tfd, HIGH_PRIO));

	/* Wait for thread_c to release. */
	__Tcall_assert(ret, evl_lock_mutex(&p->lock_c));
	__Tcall_assert(ret, evl_unlock_mutex(&p->lock_c));

	__Tcall_assert(ret, evl_unlock_mutex(&p->lock_b));
	/* We should have been deboosted to our initial priority. */
	__Texpr_assert(check_priority(tfd, MEDIUM_PRIO));

	return NULL;
}

static void *thread_a(void *arg)
{
	struct test_context *p = arg;
	int ret, tfd;

	__Tcall_assert(tfd, evl_attach_self("monitor-pi-dlk-A:%d", getpid()));
	__Tcall_assert(ret, evl_clear_thread_mode(tfd, EVL_T_WOLI, NULL));
	__Tcall_assert(ret, evl_lock_mutex(&p->lock_a));
	__Tcall_assert(ret, evl_put_sem(&p->sync));

	/* Wait for start call. */
	__Tcall_assert(ret, evl_get_sem(&p->start));
	/* Our effective priority should not have changed. */
	__Texpr_assert(check_priority(tfd, HIGH_PRIO));

	/* Wait for thread_b to release. */
	__Tcall_assert(ret, evl_lock_mutex(&p->lock_b));
	__Texpr_assert(check_priority(tfd, HIGH_PRIO));
	__Tcall_assert(ret, evl_unlock_mutex(&p->lock_b));

	__Tcall_assert(ret, evl_unlock_mutex(&p->lock_a));
	__Texpr_assert(check_priority(tfd, HIGH_PRIO));

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t t_a, t_b, t_c;
	int tfd, gfd, sfd, ret;
	struct test_context c;
	char *name;

	__Tcall_assert(tfd, evl_attach_self("monitor-pi-dlk-main:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(gfd, evl_new_mutex(&c.lock_a, name)); /* PI-implicit form. */

	name = get_unique_name(EVL_MONITOR_DEV, 1);
	__Tcall_assert(gfd, evl_new_mutex(&c.lock_b, name));

	name = get_unique_name(EVL_MONITOR_DEV, 2);
	__Tcall_assert(gfd, evl_new_mutex(&c.lock_c, name));

	name = get_unique_name(EVL_MONITOR_DEV, 3);
	__Tcall_assert(sfd, evl_new_sem(&c.sync, name));

	name = get_unique_name(EVL_MONITOR_DEV, 4);
	__Tcall_assert(sfd, evl_new_sem(&c.start, name));

	new_thread(&t_a, SCHED_FIFO, HIGH_PRIO, thread_a, &c);
	new_thread(&t_b, SCHED_FIFO, MEDIUM_PRIO, thread_b, &c);
	new_thread(&t_c, SCHED_FIFO, LOW_PRIO, thread_c, &c);

	/* Wait for all threads to synchronize with us. */
	__Tcall_assert(ret, evl_get_sem(&c.sync));
	__Tcall_assert(ret, evl_get_sem(&c.sync));
	__Tcall_assert(ret, evl_get_sem(&c.sync));

	/* Unblock A, which owns lock_a, attempts to lock lock_b. */
	__Tcall_assert(ret, evl_put_sem(&c.start));
	/* Unblock B, which owns lock_b, attempts to lock lock_c. */
	__Tcall_assert(ret, evl_put_sem(&c.start));
	/*
	 * Unblock C which owns lock_c, and should deadlock attempting
	 * to lock lock_a.
	 */
	__Tcall_assert(ret, evl_put_sem(&c.start));

	__Texpr_assert(pthread_join(t_a, NULL) == 0);
	__Texpr_assert(pthread_join(t_b, NULL) == 0);
	__Texpr_assert(pthread_join(t_c, NULL) == 0);

	evl_close_sem(&c.start);
	evl_close_sem(&c.sync);
	evl_close_mutex(&c.lock_a);
	evl_close_mutex(&c.lock_b);
	evl_close_mutex(&c.lock_c);

	return 0;
}
