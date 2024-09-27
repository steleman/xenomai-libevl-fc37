/*
 * SPDX-License-Identifier: MIT
 *
 * COMPILE-TESTING ONLY.
 */

#include <ctime>
#include <evl/syscall.h>
#include <evl/syscall-evl.h>
#include <evl/clock.h>
#include <evl/clock-evl.h>

int main(int argc, char *argv[])
{
	struct timespec ts, res;

	evl_read_clock(EVL_CLOCK_REALTIME, &ts);
	evl_set_clock(EVL_CLOCK_REALTIME, &ts);
	evl_get_clock_resolution(EVL_CLOCK_REALTIME, &res);
	evl_sleep_until(EVL_CLOCK_REALTIME, &ts);
	evl_usleep(10000);

	return 0;
}
