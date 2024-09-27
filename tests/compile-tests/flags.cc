/*
 * SPDX-License-Identifier: MIT
 *
 * COMPILE-TESTING ONLY.
 */

#include <evl/syscall.h>
#include <evl/syscall-evl.h>
#include <evl/clock.h>
#include <evl/clock-evl.h>
#include <evl/flags.h>

static DEFINE_EVL_FLAGS(static_flags);

int main(int argc, char *argv[])
{
	struct evl_flags dynamic_flags;
	struct timespec timeout;
	int bits;

	evl_new_flags(&dynamic_flags, "dynamic_flags");
	evl_create_flags(&dynamic_flags, CLOCK_MONOTONIC, 0, 0, "dynamic_flags");
	evl_open_flags(&dynamic_flags, "dynamic_flags");
	evl_close_flags(&dynamic_flags);
	evl_wait_flags(&static_flags, &bits);
	evl_wait_some_flags(&static_flags, -1, &bits);
	evl_wait_exact_flags(&static_flags, -1);
	evl_read_clock(EVL_CLOCK_MONOTONIC, &timeout);
	evl_timedwait_flags(&static_flags, &timeout, &bits);
	evl_timedwait_some_flags(&static_flags, -1, &timeout, &bits);
	evl_timedwait_exact_flags(&static_flags, -1, &timeout);
	evl_trywait_flags(&static_flags, &bits);
	evl_trywait_some_flags(&static_flags, -1, &bits);
	evl_trywait_exact_flags(&static_flags, -1);
	evl_peek_flags(&static_flags, &bits);
	evl_post_flags(&static_flags, bits);
	evl_broadcast_flags(&static_flags, bits);

	return 0;
}
