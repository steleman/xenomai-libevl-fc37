/*
 * SPDX-License-Identifier: MIT
 *
 * COMPILE-TESTING ONLY.
 */

#include <ctime>
#include <evl/syscall.h>
#include <evl/syscall-evl.h>
#include <evl/atomic.h>
#include <evl/clock.h>
#include <evl/clock-evl.h>
#include <evl/event.h>

static DEFINE_EVL_EVENT(static_event);

int main(int argc, char *argv[])
{
	struct evl_event dynamic_event;
	struct timespec timeout;
	struct evl_mutex mutex;

	evl_new_mutex(&mutex, "mutex");
	evl_new_event(&dynamic_event, "dynamic_event");
	evl_create_event(&dynamic_event, CLOCK_MONOTONIC, 0, "dynamic_event");
	evl_open_event(&dynamic_event, "dynamic_event");
	evl_close_event(&dynamic_event);
	evl_wait_event(&static_event, &mutex);
	evl_read_clock(EVL_CLOCK_MONOTONIC, &timeout);
	evl_timedwait_event(&static_event, &mutex, &timeout);
	evl_signal_event(&static_event);
	evl_broadcast_event(&static_event);
	evl_signal_thread(&static_event, -1);

	return 0;
}
