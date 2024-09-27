/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_SYS_H
#define _EVL_SYS_H

#include <stdarg.h>
#include <evl/factory.h>

#ifdef __cplusplus
extern "C" {
#endif

int evl_create_element(const char *type,
		       const char *name,
		       void *attrs,
		       int clone_flags,
		       struct evl_element_ids *eids);

int evl_open_element_vargs(const char *type,
			const char *fmt, va_list ap);

int evl_open_element(const char *type,
		     const char *path, ...);

int evl_open_raw(const char *type);

int evl_get_current_mode(void);

unsigned int evl_detect_fpu(void);

#ifdef __cplusplus
}
#endif

#endif /* _EVL_SYS_H */
