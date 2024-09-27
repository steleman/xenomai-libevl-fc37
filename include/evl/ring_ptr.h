/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2021 Philippe Gerum <rpm@xenomai.org>
 *
 * A lighweight, lock-free mp/mc FIFO ring data structure based on
 * Ruslan Nikolaev's Scalable Circular Queue (double-width CAS
 * variant):
 * http://drops.dagstuhl.de/opus/volltexte/2019/11335/pdf/LIPIcs-DISC-2019-28.pdf
 * https://github.com/rusnikola/lfqueue.git
 *
 * Copyright (C) 2019 Ruslan Nikolaev
 *
 * With k: number of concurrent readers/writers,
 *      n: ring entries (i.e. 1 << order)
 * Requirement: k <= n
 *
 * Adaptation to EVL:
 *
 * The EVL ring lives in a header-only library in order to benefit the
 * most from compile-time optimizations (typically constant
 * folding/propagation) for any given constant size. When expanded,
 * DEFINE_EVL_RINGPTR_{STATIC, DYNAMIC}() define:
 *
 * - a ring data structure type of the required size (number of entries).
 * - an API, namely the queue, dequeue and reset inline routines to
 *   manipulate that particular ring type.
 *
 * e.g. DEFINE_EVL_RINGPTR_STATIC(name=foo, order=10) defines a ring data
 * structure typenamed "foo" with 2^10 entries. The inline operations
 * generated would be:
 *
 * bool evl_enqueue_foo(void *data) 	// push next data
 * bool evl_dequeue_foo(void **pdata)	// pull heading data
 * void evl_clear_foo(void)		// clear ring
 * void evl_init_cursor_foo(struct evl_ring_cursor *cursor) // reset write cursor to ring
 *
 * Larger ring sizes may have to be dynamically allocated, in which
 * case DEFINE_EVL_RINGPTR_DYNAMIC() should be used instead. In addition
 * to the previous helpers, the following inline routine is also
 * defined:
 *
 * void evl_alloc_foo(void)		// allocate dynamic ring "foo"
 *
 * If you need to share a single ring data structure between distinct
 * compilation units, you should define it once in a particular unit,
 * then export its API which encapsulates calls to the associated
 * queue and dequeue routines.
 *
 * libatomic might be needed to use this code on some platform/gcc
 * combos in order to obtain cmpxchg16.
 */

#ifndef _EVL_RINGPTR_H
#define _EVL_RINGPTR_H

#ifdef __cplusplus
/*
 * C11 and C++ atomic APIs do not mix well at the moment.
 */
#error "ring API is not available from C++"
#endif

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <evl/compiler.h>

/*
 * Assume a large 128-byte cacheline size which fits all architectures
 * we run on. Minimum cacheline size we support is 32 bytes.
 */
#define EVL_RING_CACHELINE_SHIFT	7U
#define EVL_RING_CACHELINE_BYTES	(1U << EVL_RING_CACHELINE_SHIFT)
#define EVL_RING_ALIGNMENT		(EVL_RING_CACHELINE_BYTES * 2)

#if __WORDSIZE > 64
#error "__WORDSIZE > 64 is not supported"
#elif __WORDSIZE == 64
typedef int64_t lfsatomic_t;
typedef uint64_t lfatomic_t;
typedef __uint128_t lfatomic_big_t;
#else
typedef int32_t lfsatomic_t;
typedef uint32_t lfatomic_t;
typedef uint64_t lfatomic_big_t;
#endif

#if __WORDSIZE >= 64
#define EVL_RINGPTR_MINORDER		(EVL_RING_CACHELINE_SHIFT - 4)
#else
#define EVL_RINGPTR_MINORDER		(EVL_RING_CACHELINE_SHIFT - 3)
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#define __evl_ringptr_array_entry(__x)	((_Atomic(lfatomic_t) *)(__x) + 1)
#else
#define __evl_ringptr_array_entry(__x)	((_Atomic(lfatomic_t) *)(__x))
#endif

#define __evl_ringptr_aba_shift	(sizeof(lfatomic_big_t) * 4)
#define __evl_ringptr_aba_mask	(~(lfatomic_big_t)0U << __evl_ringptr_aba_shift)

#define __evl_ringptr_entry(__x)	((lfatomic_t)(((__x) & __evl_ringptr_aba_mask) >> __evl_ringptr_aba_shift))
#define __evl_ringptr_pointer(__x)	((lfatomic_t)(((__x) & ~__evl_ringptr_aba_mask)))
#define __evl_ringptr_pair(__e, __p)	\
	(((lfatomic_big_t)(__e) << __evl_ringptr_aba_shift) | ((lfatomic_big_t)(__p)))

#define __evl_ringptr_cmp(__x, __op, __y)	((lfsatomic_t)((__x) - (__y)) __op 0)

struct __evl_ringptr {
	__aligned(EVL_RING_CACHELINE_BYTES) _Atomic(lfatomic_t) head;
	__aligned(EVL_RING_CACHELINE_BYTES) _Atomic(lfsatomic_t) threshold;
	__aligned(EVL_RING_CACHELINE_BYTES) _Atomic(lfatomic_t) tail;
	__aligned(EVL_RING_CACHELINE_BYTES) _Atomic(lfatomic_big_t) array[0];
};

struct evl_ring_cursor {
	lfatomic_t head;
};

static inline size_t __evl_ringptr_map(lfatomic_t idx, size_t order, size_t n)
{
	return (size_t)(((idx & (n - 1)) >> (order + 1 - EVL_RINGPTR_MINORDER)) |
			((idx << EVL_RINGPTR_MINORDER) & (n - 1)));
}

#define __evl_ringptr_threshold4(__n) ((long)(2 * (__n) - 1))

#define __evl_ringptr_cells(___order)	((size_t)1U << ((___order) + 1))

static inline void __evl_ringptr_clear(struct __evl_ringptr *ring, size_t order)
{
	size_t i, n = __evl_ringptr_cells(order);

	for (i = 0; i < n; i++)
		atomic_init(&ring->array[i], 0);

	atomic_init(&ring->head, n);
	atomic_init(&ring->tail, n);
	atomic_init(&ring->threshold, -1);
}

static __always_inline
bool __evl_ringptr_enqueue(struct __evl_ringptr *ring, size_t order,
			void *ptr, struct evl_ring_cursor *cursor)
{
	size_t tidx, n = __evl_ringptr_cells(order);
	lfatomic_t tail, entry, ecycle, tcycle;
	lfatomic_big_t pair;

	tail = atomic_load(&ring->tail);
	if (tail >= cursor->head + n) {
		cursor->head = atomic_load(&ring->head);
		if (tail >= cursor->head + n)
			return false;
	}

	for (;;) {
		tail = atomic_fetch_add_explicit(&ring->tail, 1,
						memory_order_acq_rel);
		tcycle = tail & ~(lfatomic_t)(n - 1);
		tidx = __evl_ringptr_map(tail, order, n);
		pair = atomic_load_explicit(&ring->array[tidx],
					memory_order_acquire);
	retry:
		entry = __evl_ringptr_entry(pair);
		ecycle = entry & ~(lfatomic_t)(n - 1);
		if (__evl_ringptr_cmp(ecycle, <, tcycle) &&
			(entry == ecycle ||
				(entry == (ecycle | 0x2) &&
					atomic_load_explicit(&ring->head,
							memory_order_acquire) <= tail))) {
			if (!atomic_compare_exchange_weak_explicit(&ring->array[tidx],
					&pair, __evl_ringptr_pair(tcycle | 0x1, (lfatomic_t)ptr),
					memory_order_acq_rel, memory_order_acquire))
				goto retry;

			if (atomic_load(&ring->threshold) != __evl_ringptr_threshold4(n))
				atomic_store(&ring->threshold, __evl_ringptr_threshold4(n));

			return true;
		}

		if (tail + 1 >= cursor->head + n) {
			cursor->head = atomic_load(&ring->head);
			if (tail + 1 >= cursor->head + n)
				return false;
		}
	}
}

static __always_inline
void __evl_ringptr_catchup(struct __evl_ringptr *ring,
			lfatomic_t tail, lfatomic_t head)
{
	while (!atomic_compare_exchange_weak_explicit(&ring->tail, &tail, head,
				memory_order_acq_rel, memory_order_acquire)) {
		head = atomic_load_explicit(&ring->head, memory_order_acquire);
		tail = atomic_load_explicit(&ring->tail, memory_order_acquire);
		if (__evl_ringptr_cmp(tail, >=, head))
			break;
	}
}

static __always_inline
bool __evl_ringptr_dequeue(struct __evl_ringptr *ring, size_t order,
			void **ptr)
{
	lfatomic_t head, entry, entry_new, ecycle, hcycle, tail;
	size_t hidx, n = __evl_ringptr_cells(order);
	lfatomic_big_t pair;

	if (atomic_load(&ring->threshold) < 0)
		return false;

	for (;;) {
		head = atomic_fetch_add_explicit(&ring->head, 1, memory_order_acq_rel);
		hcycle = head & ~(lfatomic_t)(n - 1);
		hidx = __evl_ringptr_map(head, order, n);
		entry = atomic_load_explicit(__evl_ringptr_array_entry(&ring->array[hidx]),
					memory_order_acquire);
		do {
			ecycle = entry & ~(lfatomic_t)(n - 1);
			if (ecycle == hcycle) {
				pair = atomic_fetch_and_explicit(&ring->array[hidx],
						__evl_ringptr_pair(~(lfatomic_t) 0x1, 0),
						memory_order_acq_rel);
				*ptr = (void *)__evl_ringptr_pointer(pair);
				return true;
			}
			if ((entry & (~(lfatomic_t) 0x2)) != ecycle) {
				entry_new = entry | 0x2;
				if (entry == entry_new)
					break;
			} else {
				entry_new = hcycle | (entry & 0x2);
			}
		} while (__evl_ringptr_cmp(ecycle, <, hcycle) &&
			!atomic_compare_exchange_weak_explicit(
				__evl_ringptr_array_entry(&ring->array[hidx]),
				&entry, entry_new,
				memory_order_acq_rel, memory_order_acquire));

		tail = atomic_load_explicit(&ring->tail, memory_order_acquire);
		if (__evl_ringptr_cmp(tail, <=, head + 1)) {
			__evl_ringptr_catchup(ring, tail, head + 1);
			atomic_fetch_sub_explicit(&ring->threshold, 1,
						memory_order_acq_rel);
			return false;
		}
		if (atomic_fetch_sub_explicit(&ring->threshold, 1,
						memory_order_acq_rel) <= 0)
			return false;
	}
}

#define TYPEOF_EVL_RINGPTR(__name, __order)					\
struct __name {									\
	__aligned(EVL_RING_CACHELINE_BYTES) _Atomic(lfatomic_t) head;		\
	__aligned(EVL_RING_CACHELINE_BYTES) _Atomic(lfsatomic_t) threshold;	\
	__aligned(EVL_RING_CACHELINE_BYTES) _Atomic(lfatomic_t) tail;		\
	__aligned(EVL_RING_CACHELINE_BYTES) _Atomic(lfatomic_big_t)		\
		array[__evl_ringptr_cells(__order)];				\
} __aligned(EVL_RING_ALIGNMENT)

#define SIZEOF_EVL_RINGPTR(__order)	\
	sizeof(TYPEOF_EVL_RINGPTR(, __order))

#define __evl_ringptr_lhead(__name)	__evl_ringptr_lhead_ ## __name

#define DEFINE_EVL_RINGPTR_OPS(__name, __ring, __order)			\
									\
static inline bool							\
evl_enqueue_ ## __name (void *ptr, struct evl_ring_cursor *cursor)	\
{									\
	return __evl_ringptr_enqueue((struct __evl_ringptr *)&(__ring),	\
				__order, ptr, cursor);			\
}									\
									\
static inline bool							\
evl_dequeue_ ## __name (void **pptr)					\
{									\
	return __evl_ringptr_dequeue((struct __evl_ringptr *)&(__ring),	\
				__order, pptr);				\
}									\
									\
static inline void							\
evl_init_cursor_ ## __name (struct evl_ring_cursor *cursor)		\
{									\
	 cursor->head = __evl_ringptr_cells(__order);			\
}									\
									\
static inline void							\
evl_clear_ ## __name (void)						\
{									\
	__evl_ringptr_clear((struct __evl_ringptr *)&(__ring), __order);\
}

#define DEFINE_EVL_RINGPTR_STATIC(__name, __order)			\
	TYPEOF_EVL_RINGPTR(__name, __order) __name = {			\
		.head = __evl_ringptr_cells(__order),			\
		.tail = __evl_ringptr_cells(__order),			\
		.threshold = -1,					\
		.array = { 0 },						\
	};								\
	DEFINE_EVL_RINGPTR_OPS(__name, __name, __order)

#define DEFINE_EVL_RINGPTR_DYNAMIC(__name, __order)		\
TYPEOF_EVL_RINGPTR(__name, __order) *__name;			\
DEFINE_EVL_RINGPTR_OPS(__name, *__name, __order);		\
								\
static inline int						\
evl_alloc_ ## __name (void)					\
{								\
	void *memptr;						\
	int ret;						\
								\
	ret = posix_memalign(&memptr, EVL_RING_ALIGNMENT,	\
			SIZEOF_EVL_RINGPTR(__order));		\
	if (!ret) {						\
		__name = memptr;				\
		evl_clear_ ## __name();				\
	}							\
	return ret;						\
}

#endif /* _EVL_RINGPTR_H */
