/*
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <evl/thread.h>
#include <evl/thread-evl.h>
#include "helpers.h"

int main(int argc, char *argv[])
{
	struct sched_param param;
	int tfd, ret;
	int oldmask;

	param.sched_priority = 1;
	__Texpr_assert(pthread_setschedparam(pthread_self(),
				SCHED_FIFO, &param) == 0);

	__Tcall_assert(tfd, evl_attach_self("thread-mode-bits:%d", getpid()));

	/*
	 * Starts with no mode bit set, except maybe EVL_T_WOLI if
	 * CONFIG_EVL_DEBUG_WOLI is enabled.
	 */
	__Tcall_assert(ret, evl_set_thread_mode(tfd, EVL_T_WOSS|EVL_T_WOLI|EVL_T_WOSX, &oldmask));
	__Texpr_assert((oldmask & ~EVL_T_WOLI) == 0);
	__Tcall_assert(ret, evl_set_thread_mode(tfd, 0, &oldmask));
	__Texpr_assert(oldmask == (EVL_T_WOSS|EVL_T_WOLI|EVL_T_WOSX|EVL_T_HMSIG));
	__Tcall_assert(ret, evl_clear_thread_mode(tfd, EVL_T_WOSS, &oldmask));
	__Texpr_assert(oldmask == (EVL_T_WOSS|EVL_T_WOLI|EVL_T_WOSX|EVL_T_HMSIG));
	__Tcall_assert(ret, evl_clear_thread_mode(tfd, EVL_T_WOLI|EVL_T_WOSX, &oldmask));
	__Texpr_assert(oldmask == (EVL_T_WOLI|EVL_T_WOSX|EVL_T_HMSIG));

	__Fcall_assert(ret, evl_set_thread_mode(tfd, EVL_T_WOSS|EVL_T_WOLI|EVL_T_WOSX|EVL_T_HMOBS, &oldmask));
	__Texpr_assert(ret == -EINVAL); /* EVL_T_HMOBS should fail if self is !observable */

	__Tcall_assert(ret, evl_detach_self());
	__Tcall_assert(tfd, evl_attach_thread(EVL_CLONE_OBSERVABLE,
						"thread-mode-bits:%d", getpid()));

	__Tcall_assert(ret, evl_set_thread_mode(tfd, EVL_T_WOSS|EVL_T_WOLI|EVL_T_WOSX|EVL_T_HMOBS, &oldmask));
	__Texpr_assert((oldmask & ~EVL_T_WOLI) == 0);
	__Tcall_assert(ret, evl_set_thread_mode(tfd, EVL_T_HMSIG, &oldmask));
	__Texpr_assert(oldmask == (EVL_T_WOSS|EVL_T_WOLI|EVL_T_WOSX|EVL_T_HMOBS));
	__Tcall_assert(ret, evl_clear_thread_mode(tfd, EVL_T_HMSIG|EVL_T_HMOBS, &oldmask));

	__Tcall_assert(ret, evl_set_thread_mode(tfd, 0, &oldmask));
	__Texpr_assert(oldmask == 0);
	__Tcall_assert(ret, evl_set_thread_mode(tfd, 0, NULL));

	return 0;
}
