/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2021 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _LIB_EVL_ARM_VDSO_H
#define _LIB_EVL_ARM_VDSO_H

#define __EVL_VDSO_KVERSION	"LINUX_2.6"

#ifdef __USE_TIME_BITS64
#define __EVL_VDSO_GETTIME	"__vdso_clock_gettime64"
#else
#define __EVL_VDSO_GETTIME	"__vdso_clock_gettime"
#endif

#endif /* !_LIB_EVL_ARM_VDSO_H */
