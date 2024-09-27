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

static bool probe;

static void *rwlock_writer(void *arg)
{
	int tfd, ret;

	__Tcall_assert(tfd, evl_attach_self("rwlock-wrwait:%d", getpid()));

	/* We should fail for both read and write access. */
	__Fcall_assert(ret, evl_trylock_read(&rwlock));
	__Texpr_assert(ret == -EAGAIN);
	__Fcall_assert(ret, evl_trylock_write(&rwlock));
	__Texpr_assert(ret == -EAGAIN);

	/* Wake up the main() thread in advance. */
	__Tcall_assert(ret, evl_put_sem(&sem));

	/* Now we should block. */
	__Tcall_assert(ret, evl_lock_write(&rwlock));

	probe = true;

	/* Release and bail out. */
	__Tcall_assert(ret, evl_unlock_write(&rwlock));

	return (void *)1;
}

int main(int argc, char *argv[])
{
	struct sched_param param;
	void *status = NULL;
	int tfd, sfd, ret;
	pthread_t writer;
	char *name;

	param.sched_priority = HIGH_PRIO;
	__Texpr_assert(pthread_setschedparam(pthread_self(),
				SCHED_FIFO, &param) == 0);
	__Tcall_assert(tfd, evl_attach_self("rwlock-rd:%d", getpid()));

	name = get_unique_name(EVL_MONITOR_DEV, 0);
	__Tcall_assert(sfd, evl_new_sem(&sem, name));

	/* Get a rwlock, lock it for write immediately. */
	__Texpr_assert(evl_new_rwlock(&rwlock) == 0);
	__Tcall_assert(ret, evl_lock_write(&rwlock));

	/* Spawn the contender which also wants to write. */
	new_thread(&writer, SCHED_FIFO, LOW_PRIO, rwlock_writer, NULL);
	__Tcall_assert(ret, evl_get_sem(&sem));
	/* The writer should not have gained access yet. */
	__Texpr_assert(!probe);

	/* Release our own token. */
	__Tcall_assert(ret, evl_unlock_write(&rwlock));

	__Texpr_assert(pthread_join(writer, &status) == 0);
	__Fexpr_assert(status == NULL);

	/* Check that the writer got access eventually. */
	__Texpr_assert(probe);

	/* Check that the lock was fully released. */
	__Tcall_assert(ret, evl_trylock_write(&rwlock));

	evl_destroy_rwlock(&rwlock);
	evl_close_sem(&sem);

	return 0;
}
