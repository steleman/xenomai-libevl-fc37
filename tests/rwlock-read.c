#include <unistd.h>
#include <stdlib.h>
#include <evl/thread.h>
#include <evl/thread-evl.h>
#include <evl/rwlock.h>
#include <evl/sem.h>
#include "helpers.h"

#define LOW_PRIO	1
#define HIGH_PRIO	2

static struct evl_sem sem;

static DEFINE_EVL_RWLOCK(rwlock);

static void *rwlock_reader(void *arg)
{
	int tfd, ret;

	__Tcall_assert(tfd, evl_attach_self("rwlock-rdwait:%d", getpid()));

	/* We should be granted read acceess twice without blocking. */
	__Tcall_assert(ret, evl_lock_read(&rwlock));
	__Tcall_assert(ret, evl_lock_read(&rwlock));

	/*
	 * Wake up the main() thread which should preempt, then be
	 * denied write access.
	 */
	__Tcall_assert(ret, evl_put_sem(&sem));

	/* Release and bail out. */
	__Tcall_assert(ret, evl_unlock_read(&rwlock));
	__Tcall_assert(ret, evl_unlock_read(&rwlock));

	return (void *)1;
}

int main(int argc, char *argv[])
{
	struct sched_param param;
	void *status = NULL;
	int tfd, sfd, ret;
	pthread_t reader;
	char *name;

	param.sched_priority = HIGH_PRIO;
	__Texpr_assert(pthread_setschedparam(pthread_self(),
				SCHED_FIFO, &param) == 0);
	__Tcall_assert(tfd, evl_attach_self("rwlock-rd:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(sfd, evl_new_sem(&sem, name));

	/* Get a rwlock, lock it for read immediately. */
	__Texpr_assert(evl_new_rwlock(&rwlock) == 0);
	__Tcall_assert(ret, evl_lock_read(&rwlock));

	/* Spawn the contender which also wants to read. */
	new_thread(&reader, SCHED_FIFO, LOW_PRIO, rwlock_reader, NULL);
	__Tcall_assert(ret, evl_get_sem(&sem));
	__Fcall_assert(ret, evl_trylock_write(&rwlock));
	__Texpr(ret == -EAGAIN);

	__Texpr_assert(pthread_join(reader, &status) == 0);
	__Fexpr_assert(status == NULL);

	/* Release our own token. */
	__Tcall_assert(ret, evl_unlock_read(&rwlock));

	/* Check that the lock was fully released. */
	__Tcall_assert(ret, evl_trylock_write(&rwlock));

	evl_destroy_rwlock(&rwlock);
	evl_close_sem(&sem);

	return 0;
}
