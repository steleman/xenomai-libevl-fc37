/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2022 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_COMPAT_H
#define _EVL_COMPAT_H

#include <evl/compiler.h>
#include <evl/control.h>
#include <evl/thread.h>

#if EVL_ABI_LEVEL >= 31

#ifdef __cplusplus
extern "C" {
#endif

#define T_WOSS	T_WOSS_is_deprecated()
static inline __deprecated int T_WOSS_is_deprecated(void)
{
	return EVL_T_WOSS;
}

#define T_WOLI	T_WOLI_is_deprecated()
static inline __deprecated int T_WOLI_is_deprecated(void)
{
	return EVL_T_WOLI;
}

#define T_WOSX	T_WOSX_is_deprecated()
static inline __deprecated int T_WOSX_is_deprecated(void)
{
	return EVL_T_WOSX;
}

#define T_WOSO	T_WOSO_is_deprecated()
static inline __deprecated int T_WOSO_is_deprecated(void)
{
	return EVL_T_WOSO;
}

#define T_HMSIG	T_HMSIG_is_deprecated()
static inline __deprecated int T_HMSIG_is_deprecated(void)
{
	return EVL_T_HMSIG;
}

#define T_HMOBS	T_HMOBS_is_deprecated()
static inline __deprecated int T_HMOBS_is_deprecated(void)
{
	return EVL_T_HMOBS;
}

#ifdef __cplusplus
}
#endif

#endif /* EVL_ABI_LEVEL >= 31 */

#endif /* _EVL_COMPAT_H */
