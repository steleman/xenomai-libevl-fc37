/*
 * SPDX-License-Identifier: MIT
 *
 * Exercise the STAX serialization mechanism. A pool of threads
 * contends for holding a stax, half of them run in out-of-band mode,
 * half of them on the in-band stage. At any point in time, only
 * threads which run on the same stage should be allowed to share the
 * stax concurrently, excluding all threads from the converse stage.
 * The hectic driver provides the kernel support for locking and
 * unlocking a stax.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <error.h>
#include <errno.h>
#include <evl/thread.h>
#include <evl/thread-evl.h>
#include <evl/clock.h>
#include <evl/clock-evl.h>
#include <evl/atomic.h>
#include <evl/proxy.h>
#include <evl/proxy-evl.h>
#include <evl/syscall.h>
#include <evl/syscall-evl.h>
#include <evl/devices/hectic.h>
#include "helpers.h"

#define STAX_CONCURRENCY  8

#if STAX_CONCURRENCY > 32
#error "STAX_CONCURRENCY must be <= 32"
#endif

static int drvfd;

static atomic_t presence_mask;

static atomic_t counter_proof;

static sig_atomic_t done;

static bool verbose;

static int timeout_secs = 3;	/* Default runtime. */

static void *test_thread(void *arg)
{
	int tfd, ret, me, invalid, prev, next;
	long serial = (long)arg;
	typeof(usleep) *do_usleep;
	typeof(ioctl) *do_ioctl;
	useconds_t delay;
	bool oob;

	/*
	 * Our bit in the presence mask. Out-of-band threads have odd
	 * serial numbers in the 1-31 range, which translates to
	 * 0xAAAAAAAA in the presence mask. Conversely, in-band
	 * threads have even serial numbers in the 0-30 range, which
	 * gives 0x55555555 in the presence mask. Each side declares
	 * the other one as invalid as long as it holds the stax.
	 */
	me = 1 << serial;

	oob = !!(serial & 1);
	if (oob) {
		__Tcall_assert(tfd, evl_attach_self("stax.%ld:%d",
					serial / 2, getpid()));
		do_ioctl = oob_ioctl;
		do_usleep = evl_usleep;
		delay = 100000;
		/* Any in-band presence is invalid. */
		invalid = 0x55555555;
	} else {
		do_ioctl = ioctl;
		do_usleep = usleep;
		delay = 100000;
		/* Any oob presence is invalid. */
		invalid = 0xAAAAAAAA;
	}

	/*
	 * Do not pthread_cancel() the lock owner, this would block
	 * contenders indefinitely.
	 */
	while (!done) {
		if (atomic_load(&presence_mask) & invalid)
			atomic_fetch_add(&counter_proof, 1);

		ret = do_ioctl(drvfd, EVL_HECIOC_LOCK_STAX);
		__Texpr_assert(ret == 0);

		prev = atomic_load_explicit(&presence_mask, __ATOMIC_ACQUIRE);
		do {
			next = prev | me;
		} while (!atomic_compare_exchange_weak_explicit(
				&presence_mask, &prev, next,
			__ATOMIC_RELEASE, __ATOMIC_ACQUIRE));

		__Fexpr_assert(prev & invalid);

		do_usleep(delay);

		prev = atomic_load_explicit(&presence_mask, __ATOMIC_ACQUIRE);
		do {
			next = prev & ~me;
		} while (!atomic_compare_exchange_weak_explicit(
				&presence_mask, &prev, next,
			__ATOMIC_RELEASE, __ATOMIC_ACQUIRE));

		__Fexpr_assert(prev & invalid);

		ret = do_ioctl(drvfd, EVL_HECIOC_UNLOCK_STAX);
		__Texpr_assert(ret == 0);

		/*
		 * We should observe conflicting accesses from time to
		 * time when the stax does not guard the section.
		 */
		if (atomic_load(&presence_mask) & invalid)
			atomic_fetch_add(&counter_proof, 1);

		do_usleep(delay);
	}

	return NULL;
}

static void usage(void)
{
        fprintf(stderr, "usage: stax-lock [options]:\n");
        fprintf(stderr, "-T --timeout=<seconds>       seconds before test stops (0 means never/infinite) [=5]\n");
        fprintf(stderr, "-v --verbose                 turn on verbosity\n");
}

#define short_optlist "T:v"

static const struct option options[] = {
	{
		.name = "timeout",
		.has_arg = required_argument,
		.val = 'T',
	},
	{
		.name = "verbose",
		.has_arg = no_argument,
		.val = 'v',
	},
	{ /* Sentinel */ }
};

int main(int argc, char *argv[])
{
	pthread_t tids[STAX_CONCURRENCY];
	int ret, n, prio, policy, sig, c;
	sigset_t sigmask;

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
		case 'T':
			timeout_secs = atoi(optarg);
			if (timeout_secs < 0)
				error(1, EINVAL, "invalid timeout");
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

	/*
	 * CAUTION: this test uses an internal interface of the
	 * 'hectic' driver in order to test the stax mechanism. This
	 * interface is enabling the caller to do something wrong and
	 * nasty, i.e. holding a stax across the kernel/user space
	 * boundary. This is only for the purpose of testing this
	 * mechanism, this is bad, applications should never do this,
	 * ever. IOW, a stax should be held while in kernel space
	 * exclusively, always released before returning to user.
	 */
	drvfd = open("/dev/hectic", O_RDONLY);
	if (drvfd < 0)
		return EXIT_NO_SUPPORT;

	ret = ioctl(drvfd, EVL_HECIOC_LOCK_STAX);
	if (ret) {
		if (errno == ENOTTY)
			return EXIT_NO_SUPPORT;
		__Texpr_assert(ret == 0);
	}

	ret = ioctl(drvfd, EVL_HECIOC_UNLOCK_STAX);
	__Texpr_assert(ret == 0);

	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGTERM);
	sigaddset(&sigmask, SIGHUP);
	sigaddset(&sigmask, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &sigmask, NULL);

	if (verbose) {
		printf("starting %d threads (half in-band, half out-of-band)\n",
			STAX_CONCURRENCY);
		if (timeout_secs)
			printf("running for %d seconds...\n", timeout_secs);
		else
			printf("running indefinitely.\n");
	}

	for (n = 0; n < STAX_CONCURRENCY; n++) {
		if (n & 1) {
			prio = 1;
			policy = SCHED_FIFO;
		} else {
			prio = 0;
			policy = SCHED_OTHER;
		}
		new_thread(tids + n, policy, prio, test_thread, (void *)(long)n);
	}

	if (timeout_secs)
		alarm(timeout_secs);

	sigwait(&sigmask, &sig);
	done = true;

	for (n = 0; n < STAX_CONCURRENCY; n++)
		pthread_join(tids[n], NULL);

	/*
	 * We must have observed at least conflicting access outside
	 * of the stax protected section, otherwise we might not have
	 * tested what we thought we did...
	 */
	__Texpr_assert(atomic_load(&counter_proof) > 0);
	if (verbose)
		printf("%d legit conflicts detected\n", atomic_load(&counter_proof));

	return 0;
}
