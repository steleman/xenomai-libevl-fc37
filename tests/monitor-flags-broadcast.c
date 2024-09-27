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
#include <evl/flags.h>
#include <evl/clock.h>
#include <evl/clock-evl.h>
#include <evl/poll.h>
#include <evl/poll-evl.h>
#include <evl/sem.h>
#include "helpers.h"

#define NR_RECEIVERS  4

#define LOW_PRIO   1
#define HIGH_PRIO  2

static struct evl_flags flags;

static struct evl_sem start;

static void *flags_receiver(void *arg)
{
	int nth = (int)(long)arg, ret, tfd, n = 0;
	unsigned int bits;

	__Tcall_assert(tfd, evl_attach_self("monitor-flags-receiver:%d.%d",
					    getpid(), nth));
	__Tcall_assert(ret, evl_put_sem(&start));

	do {
		__Tcall_assert(ret, evl_wait_flags(&flags, &bits));
		__Texpr_assert(bits == (1U << n));
		n++;
	} while (bits != 0x80000000);

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t receivers[NR_RECEIVERS];
	int tfd, ret, n, pollfd, ffd, sfd;
	struct evl_poll_event pollset;
	struct sched_param param;
	void *status = NULL;
	char *name;

	param.sched_priority = LOW_PRIO;
	__Texpr_assert(pthread_setschedparam(pthread_self(),
				SCHED_FIFO, &param) == 0);

	/* EVL inherits the inband scheduling params upon attachment. */
	__Tcall_assert(tfd, evl_attach_self("monitor-flags-broadcast:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(ffd, evl_new_flags(&flags, name));

	name = get_unique_name(EVL_MONITOR_DEV, 1);
	__Tcall_assert(sfd, evl_new_sem(&start, name));

	__Tcall_assert(pollfd, evl_new_poll());
	__Tcall_assert(ret, evl_add_pollfd(pollfd, ffd, POLLOUT, evl_nil));

	for (n = 0; n < NR_RECEIVERS; n++) {
		new_thread(receivers + n, SCHED_FIFO, HIGH_PRIO,
			flags_receiver, (void *)(long)n);
		__Tcall_assert(ret, evl_get_sem(&start));
	}

	for (n = 1; n != 0; n <<= 1) {
		/* Wait for the flag group to be clear. */
		__Tcall_assert(ret, evl_poll(pollfd, &pollset, 1));
		__Texpr_assert(ret == 1L);
		__Texpr_assert(pollset.events == POLLOUT);
		__Texpr_assert((int)pollset.fd == ffd);
		/* Then post the next pattern to all waiters. */
		__Tcall_assert(ret, evl_broadcast_flags(&flags, n));
	}

	for (n = 0; n < NR_RECEIVERS; n++) {
		__Texpr_assert(pthread_join(receivers[n], &status) == 0);
		__Texpr_assert(status == NULL);
	}

	__Tcall_assert(ret, evl_close_flags(&flags));
	__Tcall_assert(ret, evl_close_sem(&start));

	return 0;
}
