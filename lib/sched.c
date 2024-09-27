/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <evl/thread.h>
#include <evl/thread-evl.h>
#include <evl/syscall.h>
#include <evl/syscall-evl.h>
#include <evl/sched.h>
#include <evl/sched-evl.h>
#include <evl/control.h>
#include "internal.h"

int evl_set_schedattr(int efd, const struct evl_sched_attrs *attrs)
{
	return __evl_conforming_io(efd, ioctl, EVL_THRIOC_SET_SCHEDPARAM, attrs);
}

int evl_get_schedattr(int efd, struct evl_sched_attrs *attrs)
{
	return __evl_conforming_io(efd, ioctl, EVL_THRIOC_GET_SCHEDPARAM, attrs);
}

int evl_control_sched(int policy,
		const union evl_sched_ctlparam *param,
		union evl_sched_ctlinfo *info,
		int cpu)
{
	struct evl_sched_ctlreq ctlreq;

	if (__evl_ctlfd < 0)
		return -ENXIO;

	ctlreq.policy = policy;
	ctlreq.cpu = cpu;
	ctlreq.param_ptr = __evl_ptr64(param);
	ctlreq.info_ptr = __evl_ptr64(info);

	return __evl_conforming_io(__evl_ctlfd, ioctl, EVL_CTLIOC_SCHEDCTL, &ctlreq);
}

int evl_get_cpustate(int cpu, int *state_r)
{
	struct evl_cpu_state cpst;
	__u32 state;
	int ret;

	if (__evl_ctlfd < 0)
		return -ENXIO;

	cpst.cpu = cpu;
	cpst.state_ptr = __evl_ptr64(&state);

	ret = __evl_conforming_io(__evl_ctlfd, ioctl, EVL_CTLIOC_GET_CPUSTATE, &cpst);
	if (ret)
		return ret;

	*state_r = (int)state;

	return 0;
}

int evl_yield(void)
{
	if (__evl_current == EVL_NO_HANDLE)
		return -EPERM;

	/* This is our sched_yield(). */
	return oob_ioctl(__evl_current_efd, EVL_THRIOC_YIELD) ? -errno : 0;
}
