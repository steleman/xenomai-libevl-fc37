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
#include <evl/sem.h>

static DEFINE_EVL_SEM(static_sem);

int main(int argc, char *argv[])
{
	struct timespec timeout;
	struct evl_sem dynamic_sem;
	int val;

	evl_new_sem(&dynamic_sem, "dynamic_sem");
	evl_create_sem(&dynamic_sem, CLOCK_MONOTONIC, 0, 0, "dynamic-sem");
	evl_open_sem(&dynamic_sem, "dynamic_sem");
	evl_close_sem(&dynamic_sem);
	evl_get_sem(&static_sem);
	evl_read_clock(EVL_CLOCK_MONOTONIC, &timeout);
	evl_timedget_sem(&static_sem, &timeout);
	evl_tryget_sem(&static_sem);
	evl_peek_sem(&static_sem, &val);
	evl_put_sem(&static_sem);

	return 0;
}
