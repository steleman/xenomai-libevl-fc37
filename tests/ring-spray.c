/*
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <error.h>
#include <stdio.h>
#include <evl/ring_ptr.h>
#include "helpers.h"

#define MAX_FEEDERS 32
#define RING_ORDER  20

#define MAX_CELLS   (1U << RING_ORDER)

#if (MAX_CELLS / MAX_FEEDERS) * MAX_FEEDERS != MAX_CELLS
#error "pow2(RING_ORDER) must be a multiple of MAX_FEEDERS"
#endif

#if RING_ORDER < 3
#error "too few ring cells (RING_ORDER >= 3)"
#endif

#if MAX_FEEDERS + 1 > MAX_CELLS
#error "too many feeders (MAX_FEEDERS + 1 <= pow2(RING_ORDER))"
#endif

DEFINE_EVL_RINGPTR_DYNAMIC(ring_spray, RING_ORDER);

static pthread_t tid[MAX_FEEDERS];

static pthread_barrier_t barrier;

static void *results[MAX_CELLS];

static long maxcpus;

static int set_thread_affinity(int nr)
{
	cpu_set_t affinity;
	int ret, cpu;

	cpu = nr % maxcpus;
	CPU_ZERO(&affinity);
	CPU_SET(cpu, &affinity);
	__Tcall_assert(ret, sched_setaffinity(0, sizeof(affinity), &affinity));

	return cpu;
}

#include <asm/unistd.h>

static void *feeder(void *arg)
{
	unsigned int nr = (int)(long)arg, n;
	struct evl_ring_cursor cursor;
	void *ptr;

	set_thread_affinity(nr);
	pthread_barrier_wait(&barrier);
	evl_init_cursor_ring_spray(&cursor);

	for (n = 0; n < MAX_CELLS / MAX_FEEDERS; n++) {
		ptr = (void *)(long)((nr << 24)|n);
		evl_enqueue_ring_spray(ptr, &cursor);
	}

	return NULL;
}

static int compare(const void *lhs, const void *rhs)
{
	return *(long *)lhs - *(long *)rhs;
}

int main(int argc, char *argv[])
{
	unsigned int n, m;
	void *ptr;
	int ret;

	/* XXX: online CPUs might not be subsequent. Oh, well. */
	maxcpus = sysconf(_SC_NPROCESSORS_ONLN);

	/*
	 * We use the dynamic allocation form to keep the exec size
	 * small by not overcrowding the .data section with a massive
	 * ring. DEFINE_EVL_RINGPTR_STATIC() is fine for smaller
	 * rings.
	 */
	ret = evl_alloc_ring_spray();
	if (ret)
		error(1, ret, "evl_alloc_ring_spray()");

	pthread_barrier_init(&barrier, NULL, MAX_FEEDERS + 1);

	for (n = 0; n < MAX_FEEDERS; n++)
		__Texpr_assert(pthread_create(tid + n, NULL, feeder,
				(void *)(long)n) == 0);

	pthread_barrier_wait(&barrier);

	for (n = 0; n < MAX_CELLS; n++) {
		while (!evl_dequeue_ring_spray(&ptr))
			usleep(100);
		results[n] = ptr;
	}

	for (n = 0; n < MAX_FEEDERS; n++)
		__Texpr_assert(pthread_join(tid[n], NULL) == 0);

	qsort(results, MAX_CELLS, sizeof(ptr), compare);

	for (n = 0; n < MAX_CELLS; n++) {
		ptr = results[n];
		m = (n / (MAX_CELLS / MAX_FEEDERS)) << 24;
		__Texpr_assert((unsigned long)ptr == (m | (n % (MAX_CELLS / MAX_FEEDERS))));
	}

	return 0;
}
