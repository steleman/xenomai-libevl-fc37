/*
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <stdio.h>
#include <error.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <evl/atomic.h>
#include <evl/evl.h>
#include <evl/clock.h>
#include <evl/clock-evl.h>
#include <evl/observable.h>
#include <evl/observable-evl.h>
#include <evl/mutex.h>
#include <evl/mutex-evl.h>
#include <evl/sched.h>
#include <evl/sched-evl.h>
#include <evl/thread.h>
#include <evl/thread-evl.h>
#include "helpers.h"

#define LOW_PRIO	1
#define HIGH_PRIO	2

#define NR_WINDOWS	4

#define do_trace(__fmt, __args...)				\
	do {							\
		if (verbose)					\
			evl_printf(__fmt "\n", ##__args);	\
	} while (0)

static pthread_t threadA, threadB;

static struct evl_sem sync_sem, start_sem;

static int test_cpu = 1;

static bool verbose;

static void usage(void)
{
        fprintf(stderr, "usage: sched-tp-overrun [options]:\n");
        fprintf(stderr, "-c --cpu           run test on given CPU [=1]\n");
        fprintf(stderr, "-v --verbose       turn on verbosity\n");
}

#define short_optlist "vc:"

#define ALLOTTED_TIME	1000000	/* ns */
#define OVERRUN_TIME	(ALLOTTED_TIME + 200000)
#define BREATHING_TIME	2000000	/* ns */

static const struct option options[] = {
	{
		.name = "cpu",
		.has_arg = required_argument,
		.val = 'c',
	},
	{
		.name = "verbose",
		.has_arg = no_argument,
		.val = 'v',
	},
	{ /* Sentinel */ }
};

static void set_thread_affinity(void)
{
	cpu_set_t affinity;
	int ret;

	CPU_ZERO(&affinity);
	CPU_SET(test_cpu, &affinity);
	__Tcall_assert(ret, sched_setaffinity(0, sizeof(affinity), &affinity));
}

static void *tp_thread(void *arg)
{
	int ret, part = (int)(long)arg, tfd;
	struct evl_sched_attrs attrs;
	struct evl_notification nf;
	struct timespec start, now;

	set_thread_affinity();

	__Tcall_assert(tfd, evl_attach_thread(EVL_CLONE_OBSERVABLE|EVL_CLONE_NONBLOCK,
						"sched-tp-overrun:%d.%d",
						getpid(), part));
	/* This is introspection, we subscribe to our own HM event stream. */
	__Tcall_assert(ret, evl_subscribe(tfd, 16, 0));

	/*
	 * Enable Schedule Overrun (EVL_T_WOSO) notification via
	 * observable.
	 */
	__Tcall_assert(ret, evl_set_thread_mode(tfd, EVL_T_WOSO|EVL_T_HMOBS, NULL));

	__Tcall_assert(ret, evl_put_sem(&sync_sem));

	attrs.sched_policy = SCHED_TP;
	attrs.sched_priority = 10 - part;
	attrs.sched_tp_partition = part;
	__Tcall_assert(ret, evl_set_schedattr(tfd, &attrs));

	__Tcall_assert(ret, evl_get_sem(&start_sem));

	evl_read_clock(EVL_CLOCK_MONOTONIC, &start);

	/*
	 * Trigger an overrun condition by staying busy for longer
	 * than the duration of our allotted time window.
	 */
	for (;;) {
		evl_read_clock(EVL_CLOCK_MONOTONIC, &now);
		if (timespec_sub_ns(&now, &start) > OVERRUN_TIME)
			break;
	}

	/*
	 * Now check whether we received the overrun event as
	 * expected. Since non-blocking input is enabled, a lack of
	 * notification would amount to an error (EAGAIN).
	 */
	__Tcall_assert(ret, evl_read_observable(tfd, &nf, 1));
	__Texpr_assert(ret == 1);
	__Texpr_assert(nf.tag == EVL_HMDIAG_OVERRUN);

	/*
	 * threadA in partition #0 always (over)runs in window #0,
	 * threadB in partition #1 in window #2. So we expect the
	 * overrun window to be equal to two times the partition
	 * number the current thread is assigned to.
	 */
	__Texpr_assert(nf.event.val == part * 2);

	/* Check that a single notification was sent. */
	__Fcall_assert(ret, evl_read_observable(tfd, &nf, 1));
	__Texpr_assert(ret == -EAGAIN);

	return NULL;
}

int main(int argc, char *argv[])
{
	union evl_sched_ctlparam *p;
	struct sched_param param;
	int tfd, ret, c;
	size_t len;

	for (;;) {
		c = getopt_long(argc, argv, short_optlist, options, NULL);
		if (c == EOF)
			break;

		switch (c) {
		case 0:
			break;
		case 'v':
			verbose = true;
			break;
		case 'c':
			test_cpu = atoi(optarg);
			if (test_cpu < 0 || test_cpu >= CPU_SETSIZE)
				error(1, EINVAL, "invalid CPU number");
			break;
		case '?':
		default:
			usage();
			return 1;
		}
	}

	if (optind < argc) {
		usage();
		return 1;
	}

	param.sched_priority = HIGH_PRIO;
	__Texpr_assert(pthread_setschedparam(pthread_self(),
				SCHED_FIFO, &param) == 0);

	/* EVL inherits the inband scheduling params upon attachment. */
	__Tcall_assert(tfd, evl_attach_self("tp-overrun-main:%d", getpid()));

	__Tcall_assert(ret, evl_new_sem(&sync_sem, "tp-overrun-sync:%d", getpid()));
	__Tcall_assert(ret, evl_new_sem(&start_sem, "tp-overrun-start:%d", getpid()));

	/*
	 * We need all threads to be pinned on the test CPU, so that
	 * evl_set_schedattr() implicitly refers to the TP schedule we
	 * are about to install on that particular processor.
	 */
	test_cpu = pick_test_cpu(test_cpu, false, NULL);
	do_trace("picked CPU%d for execution", test_cpu);
	set_thread_affinity();

	len = evl_tp_paramlen(NR_WINDOWS);
	p = malloc(len);
	if (p == NULL)
		error(1, ENOMEM, "malloc");

	/* 5 ms global time frame */
	p->tp.nr_windows = NR_WINDOWS;
	p->tp.windows[0].offset.tv_sec = 0;
	p->tp.windows[0].offset.tv_nsec = 0;
	p->tp.windows[0].duration.tv_sec = 0;
	p->tp.windows[0].duration.tv_nsec = ALLOTTED_TIME;
	p->tp.windows[0].ptid = 0;
	p->tp.windows[1].offset.tv_sec = 0;
	p->tp.windows[1].offset.tv_nsec = ALLOTTED_TIME;
	p->tp.windows[1].duration.tv_sec = 0;
	p->tp.windows[1].duration.tv_nsec = BREATHING_TIME; /* Breathe for 2ms */
	p->tp.windows[1].ptid = EVL_TP_IDLE;
	p->tp.windows[2].offset.tv_sec = 0;
	p->tp.windows[2].offset.tv_nsec = ALLOTTED_TIME + BREATHING_TIME;
	p->tp.windows[2].duration.tv_sec = 0;
	p->tp.windows[2].duration.tv_nsec = ALLOTTED_TIME;
	p->tp.windows[2].ptid = 1;
	p->tp.windows[3].offset.tv_sec = 0;
	p->tp.windows[3].offset.tv_nsec = ALLOTTED_TIME * 2 + BREATHING_TIME;
	p->tp.windows[3].duration.tv_sec = 0;
	p->tp.windows[3].duration.tv_nsec = BREATHING_TIME; /* Breathe again */
	p->tp.windows[3].ptid = EVL_TP_IDLE;
	p->tp.op = evl_tp_install;
	__Tcall_assert(ret, evl_control_sched(SCHED_TP, p, NULL, test_cpu));

	/* Start the TP schedule. */
	p->tp.op = evl_tp_start;
	__Tcall_assert(ret, evl_control_sched(SCHED_TP, p, NULL, test_cpu));
	free(p);

	new_thread(&threadA, SCHED_FIFO, LOW_PRIO, tp_thread, (void *)0L);
	__Tcall_assert(ret, evl_get_sem(&sync_sem));
	new_thread(&threadB, SCHED_FIFO, LOW_PRIO, tp_thread, (void *)1L);
	__Tcall_assert(ret, evl_get_sem(&sync_sem));

	__Tcall_assert(ret, evl_put_sem(&start_sem));
	__Tcall_assert(ret, evl_put_sem(&start_sem));

	pthread_join(threadB, NULL);
	pthread_join(threadA, NULL);

	return 0;
}
