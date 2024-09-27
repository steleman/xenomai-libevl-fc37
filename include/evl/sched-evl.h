/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_SCHED_EVL_H
#define _EVL_SCHED_EVL_H

#include <sys/types.h>
#include <sched.h>
#include <evl/sched.h>

#ifdef __cplusplus
extern "C" {
#endif

int evl_set_schedattr(int efd, const struct evl_sched_attrs *attrs);

int evl_get_schedattr(int efd, struct evl_sched_attrs *attrs);

int evl_control_sched(int policy,
		const union evl_sched_ctlparam *param,
		union evl_sched_ctlinfo *info,
		int cpu);

int evl_get_cpustate(int cpu, int *state_r);

int evl_yield(void);

#ifdef __cplusplus
}
#endif

#endif /* _EVL_SCHED_H */
