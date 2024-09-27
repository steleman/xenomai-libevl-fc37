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

struct test_context {
	struct evl_sem sem;
};

static void *sem_poster(void *arg)
{
	struct test_context *p = arg;
	int ret, tfd;

	__Tcall_assert(tfd, evl_attach_self("sem-wait-contend:%d", getpid()));
	__Tcall_assert(ret, evl_put_sem(&p->sem));

	return NULL;
}

int main(int argc, char *argv[])
{
	struct test_context c;
	pthread_t poster;
	int tfd, sfd, ret;
	char *name;

	__Tcall_assert(tfd, evl_attach_self("sem-close-unblock:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(sfd, evl_new_sem(&c.sem, name));
	new_thread(&poster, SCHED_FIFO, 1, sem_poster, &c);

	__Tcall_assert(ret, evl_get_sem(&c.sem));
	pthread_join(poster, NULL);

	evl_close_sem(&c.sem);

	return 0;
}
