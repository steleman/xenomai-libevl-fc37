/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <linux/types.h>
#include <valgrind/valgrind.h>
#include <evl/atomic.h>
#include <evl/evl.h>
#include <evl/sys.h>
#include <evl/syscall.h>
#include <evl/thread.h>
#include <evl/signal.h>
#include <evl/control.h>
#include <asm/evl/vdso.h>
#include "parse_vdso.h"
#include "internal.h"

#ifndef EVL_ABI_BASE
/*
 * EVL_ABI_BASE was not defined prior to ABI 18, until support for ABI
 * ranges was introduced in the core.
 */
#define EVL_ABI_BASE  17
#else
#endif
#if !(EVL_ABI_PREREQ >= EVL_ABI_BASE && EVL_ABI_PREREQ <= EVL_ABI_LEVEL)
#error "kernel does not meet our ABI requirements (uapi vs EVL_ABI_PREREQ)"
#endif

static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static int init_status;

static struct evl_core_info core_info;

int __evl_ctlfd = -1;

void *__evl_shared_memory = NULL;

static void atfork_unmap_shmem(void)
{
	if (__evl_shared_memory) {
		munmap(__evl_shared_memory, core_info.shm_size);
		__evl_shared_memory = NULL;
	}

	if (__evl_ctlfd >= 0) {
		close(__evl_ctlfd);
		__evl_ctlfd = -1;
	}

	init_once = PTHREAD_ONCE_INIT;
}

static inline int generic_init(void)
{
	int ctlfd, ret;
	void *shmem;

	/*
	 * Failing to open the control device with ENOENT is a clear
	 * sign that we have no EVL core in there. Return with -ENOSYS
	 * to give a clear hint about this.
	 */
	ctlfd = open(EVL_CONTROL_DEV, O_RDWR);
	if (ctlfd < 0) {
		if (errno == ENOENT) {
			fprintf(stderr,	"evl: core not enabled in kernel\n");
			return -ENOSYS;
		}
		return -errno;
	}

	ret = fcntl(ctlfd, F_GETFD, 0);
	if (ret < 0) {
		ret = -errno;
		goto fail;
	}

	ret = fcntl(ctlfd, F_SETFD, ret | O_CLOEXEC);
	if (ret < 0) {
		ret = -errno;
		goto fail;
	}

	ret = ioctl(ctlfd, EVL_CTLIOC_GET_COREINFO, &core_info);
	if (ret) {
		/*
		 * sizeof(core_info) is encoded into
		 * EVL_CTLIOC_GET_COREINFO, in which case we might
		 * receive ENOTTY if some ABI change involved updating
		 * the core info struct itself.
		 */
		if (errno != ENOTTY) {
			ret = -errno;
			goto fail;
		}
		/*
		 * core_info was not filled in, make sure we catch the
		 * ABI discrepancy.
		 */
		core_info.abi_base = (__u32)-1;
	}

	if (EVL_ABI_PREREQ < core_info.abi_base ||
		EVL_ABI_PREREQ > core_info.abi_current) {
		fprintf(stderr,
			"evl: ABI mismatch, see -ENOEXEC at https://evlproject.org/"
			"core/user-api/init/#evl_init\n");
		ret = -ENOEXEC;
		goto fail;
	}

	ret = __evl_attach_clocks();
	if (ret)
		goto fail;

	shmem = mmap(NULL, core_info.shm_size, PROT_READ|PROT_WRITE,
		MAP_SHARED, ctlfd, 0);
	if (shmem == MAP_FAILED) {
		ret = -errno;
		goto fail;
	}

	pthread_atfork(NULL, NULL, atfork_unmap_shmem);
	__evl_ctlfd = ctlfd;
	__evl_shared_memory = shmem;

	return 0;
fail:
	close(ctlfd);

	return ret;
}

#define raw_write_out(__msg)					\
	do {							\
		int __ret;					\
		__ret = write(1, __msg , strlen(__msg));	\
		(void)__ret;					\
	} while (0)

static const char *sigdebug_msg[] = {
	[EVL_HMDIAG_SIGDEMOTE] = "switched inband (signal)\n",
	[EVL_HMDIAG_SYSDEMOTE] = "switched inband (syscall)\n",
	[EVL_HMDIAG_EXDEMOTE] = "switched inband (fault)\n",
	[EVL_HMDIAG_LKDEPEND] = "switched inband while holding mutex\n",
	[EVL_HMDIAG_WATCHDOG] = "watchdog triggered\n",
	[EVL_HMDIAG_LKIMBALANCE] = "mutex lock/unlock imbalance\n",
	[EVL_HMDIAG_LKSLEEP] = "attempt to sleep while holding a mutex\n",
	[EVL_HMDIAG_STAGEX] = "locked out from out-of-band stage (stax)\n",
};

/* A basic SIGDEBUG handler which only prints out the cause. */

void evl_sigdebug_handler(int sig, siginfo_t *si, void *ctxt)
{
	if (sigdebug_marked(si)) {
		switch (sigdebug_cause(si)) {
		case EVL_HMDIAG_SIGDEMOTE:
		case EVL_HMDIAG_SYSDEMOTE:
		case EVL_HMDIAG_EXDEMOTE:
		case EVL_HMDIAG_LKDEPEND:
		case EVL_HMDIAG_WATCHDOG:
		case EVL_HMDIAG_LKIMBALANCE:
		case EVL_HMDIAG_LKSLEEP:
		case EVL_HMDIAG_STAGEX:
			raw_write_out(sigdebug_msg[sigdebug_cause(si)]);
			break;
		}
	}
}

static void resolve_vdso_calls(void)
{
	/*
	 * We have no vDSO if running on Valgrind, always use fallback
	 * calls.
	 */
	if (RUNNING_ON_VALGRIND)
		return;

	evl_init_vdso();

	__evl_clock_gettime = evl_request_vdso(__EVL_VDSO_KVERSION,
					__EVL_VDSO_GETTIME);
}

static int do_init(void)
{
	int ret;

	resolve_vdso_calls();

	ret = mlockall(MCL_CURRENT | MCL_FUTURE);
	if (ret)
		return -errno;

	ret = generic_init();
	if (ret)
		return ret;

	__evl_setup_proxies();

	return 0;
}

static void do_init_once(void)
{
	init_status = do_init();
}

int evl_init(void)
{
	pthread_once(&init_once, do_init_once);

	return init_status;
}

unsigned int evl_detect_fpu(void)
{
	if (evl_init())
		return 0;

	return core_info.fpu_features;
}
