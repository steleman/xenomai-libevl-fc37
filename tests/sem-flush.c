/*
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <evl/thread.h>
#include <evl/thread-evl.h>
#include <evl/clock.h>
#include <evl/clock-evl.h>
#include <evl/sem.h>
#include "helpers.h"

#define NR_WAITERS 5

static struct evl_sem start;

static struct evl_sem sem;

static void *sem_waiter(void *arg)
{
	int nth = (int)(long)arg, ret, tfd;

	__Tcall_assert(tfd, evl_attach_self("sem-flush-waiter:%d.%d", getpid(), nth));
	__Tcall_assert(ret, evl_put_sem(&start));
	__Fcall_assert(ret, evl_get_sem(&sem));
	__Texpr_assert(ret == -EAGAIN);

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t waiters[NR_WAITERS];
	int tfd, sfd, ret, n;
	char *name;

	__Tcall_assert(tfd, evl_attach_self("sem-flush:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(sfd, evl_new_sem(&start, name));
	name = get_unique_name(EVL_MONITOR_DEV, 1);
	__Tcall_assert(sfd, evl_new_sem(&sem, name));

	for (n = 0; n < NR_WAITERS; n++) {
		new_thread(waiters + n, SCHED_FIFO, 1, sem_waiter, (void *)(long)n);
		__Tcall_assert(ret, evl_get_sem(&start));
	}

	__Tcall_assert(ret, evl_flush_sem(&sem));

	for (n = 0; n < NR_WAITERS; n++)
		pthread_join(waiters[n], NULL);

	evl_close_sem(&sem);
	evl_close_sem(&start);

	return 0;
}
