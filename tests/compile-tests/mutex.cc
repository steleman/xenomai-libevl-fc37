/*
 * SPDX-License-Identifier: MIT
 *
 * COMPILE-TESTING ONLY.
 */

#include <evl/atomic.h>
#include <evl/syscall.h>
#include <evl/syscall-evl.h>
#include <evl/clock.h>
#include <evl/clock-evl.h>
#include <evl/mutex.h>
#include <evl/mutex-evl.h>

static DEFINE_EVL_MUTEX(static_mutex);

int main(int argc, char *argv[])
{
	struct evl_mutex dynamic_mutex;
	struct timespec timeout;

	evl_new_mutex(&dynamic_mutex, "dynamic_mutex");
	evl_create_mutex(&dynamic_mutex, CLOCK_MONOTONIC, 0,
			  EVL_MUTEX_NORMAL, "dynamic_mutex");
	evl_open_mutex(&dynamic_mutex, "dynamic_mutex");
	evl_close_mutex(&dynamic_mutex);
	evl_lock_mutex(&dynamic_mutex);
	evl_read_clock(EVL_CLOCK_MONOTONIC, &timeout);
	evl_timedlock_mutex(&dynamic_mutex, &timeout);
	evl_trylock_mutex(&static_mutex);
	evl_unlock_mutex(&static_mutex);
	evl_set_mutex_ceiling(&static_mutex, 0);
	evl_get_mutex_ceiling(&static_mutex);

	return 0;
}
