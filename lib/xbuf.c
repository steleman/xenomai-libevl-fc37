/*
 * SPDX-License-Identifier: MIT
 *
 * 2018 - Philippe Gerum <rpm@xenomai.org>
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <evl/sys.h>
#include <evl/xbuf-evl.h>

int evl_create_xbuf(size_t i_bufsz, size_t o_bufsz,
		int flags, const char *fmt, ...)
{
	struct evl_xbuf_attrs attrs;
	char *name = NULL;
	int ret, efd;
	va_list ap;

	if (fmt) {
		va_start(ap, fmt);
		ret = vasprintf(&name, fmt, ap);
		va_end(ap);
		if (ret < 0)
			return -ENOMEM;
	}

	attrs.i_bufsz = i_bufsz;
	attrs.o_bufsz = o_bufsz;
	efd = evl_create_element(EVL_XBUF_DEV, name, &attrs, flags, NULL);
	if (name)
		free(name);

	return efd;
}
