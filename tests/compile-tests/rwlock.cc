/*
 * SPDX-License-Identifier: MIT
 *
 * COMPILE-TESTING ONLY.
 */

#include <evl/rwlock.h>

static DEFINE_EVL_RWLOCK(rwlock_static);

int main(int argc, char *argv[])
{
	struct evl_rwlock rwlock;

	evl_lock_read(&rwlock_static);
	evl_new_rwlock(&rwlock);
	evl_create_rwlock(&rwlock);
	evl_destroy_rwlock(&rwlock);
	evl_lock_read(&rwlock);
	evl_trylock_read(&rwlock);
	evl_unlock_read(&rwlock);
	evl_lock_write(&rwlock);
	evl_trylock_write(&rwlock);
	evl_unlock_write(&rwlock);

	return 0;
}
