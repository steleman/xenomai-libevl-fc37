/*
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <time.h>
#include <poll.h>
#include <semaphore.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <evl/atomic.h>
#include <evl/evl.h>
#include <evl/flags.h>
#include <evl/thread.h>
#include <evl/thread-evl.h>
#include "helpers.h"

#define LOW_PRIO   1

static int i_ffd, o_ffd;

static struct evl_flags i_flags, o_flags;

static sem_t i_start, o_start;

static void *inband_receiver(void *arg)
{
	int n = 0, ret;
	__s32 bits;

	__Tcall_assert(ret, sem_post(&i_start));

	do {
		__Tcall_assert(ret, read(i_ffd, &bits, sizeof(bits)));
		__Texpr_assert(bits == (1 << n));
		n++;
	} while ((__u32)bits != 0x80000000);

	return NULL;
}

static void *oob_receiver(void *arg)
{
	int n = 0, tfd, ret;
	__s32 bits;

	__Tcall_assert(tfd, evl_attach_self("monitor-flags-oob-receiver:%d",
					    getpid()));
	__Tcall_assert(ret, sem_post(&o_start));

	do {
		__Tcall_assert(ret, evl_wait_flags(&i_flags, &bits));
		__Texpr_assert(bits == (1 << n));
		__Tcall_assert(ret, evl_post_flags(&o_flags, bits));
		n++;
	} while ((__u32)bits != 0x80000000);

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t i_receiver, o_receiver;
	struct pollfd pollfd;
	void *status;
	__s32 n, bits;
	char *name;
	int ret;

	sem_init(&i_start, 0, 0);

	__Tcall_assert(ret, evl_init());
	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(i_ffd, evl_new_flags(&i_flags, name));
	name = get_unique_name(EVL_MONITOR_DEV, 1);
	__Tcall_assert(o_ffd, evl_new_flags(&o_flags, name));

	new_thread(&i_receiver, SCHED_OTHER, 0, inband_receiver, NULL);
	__Tcall_assert(ret, sem_wait(&i_start));

	/* Try in-band -> in-band */

	for (n = 1; n != 0; n <<= 1) {
		/* Wait for the flag group to be clear. */
		pollfd.fd = i_ffd;
		pollfd.events = POLLOUT;
		pollfd.revents = 0;
		__Tcall_assert(ret, poll(&pollfd, 1, -1));
		__Texpr_assert(ret == 1);
		__Texpr_assert(pollfd.revents == POLLOUT);
		__Texpr_assert(pollfd.fd == i_ffd);
		/* Then post the next pattern. */
		__Tcall_assert(ret, write(i_ffd, &n, sizeof(n)));
	}

	__Texpr_assert(pthread_join(i_receiver, &status) == 0);
	__Texpr_assert(status == NULL);

	new_thread(&o_receiver, SCHED_FIFO, 1, oob_receiver, NULL);
	__Tcall_assert(ret, sem_wait(&o_start));

	/* Try in-band -> oob and back. */

	for (n = 1; n != 0; n <<= 1) {
		/* Wait for the flag group to be clear. */
		pollfd.fd = i_ffd;
		pollfd.events = POLLOUT;
		pollfd.revents = 0;
		__Tcall_assert(ret, poll(&pollfd, 1, -1));
		__Texpr_assert(ret == 1);
		__Texpr_assert(pollfd.revents == POLLOUT);
		__Texpr_assert(pollfd.fd == i_ffd);
		/* Post the next pattern. */
		__Tcall_assert(ret, write(i_ffd, &n, sizeof(n)));
		/* Read the echo back. */
		__Tcall_assert(ret, read(o_ffd, &bits, sizeof(bits)));
		__Texpr_assert(bits == n);
	}

	__Texpr_assert(pthread_join(o_receiver, &status) == 0);
	__Texpr_assert(status == NULL);

	__Tcall_assert(ret, close(i_ffd));
	__Tcall_assert(ret, close(o_ffd));

	return 0;
}
