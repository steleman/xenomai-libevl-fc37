/*
 * SPDX-License-Identifier: MIT
 *
 * COMPILE-TESTING ONLY.
 */

#include <stdarg.h>
#include <evl/proxy.h>
#include <evl/proxy-evl.h>

static void do_vprint(int efd, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	evl_vprint_proxy(efd, fmt, ap);
	va_end(ap);
}

int main(int argc, char *argv[])
{
	int efd;

	efd = evl_new_proxy(1, 8192, "test-proxy");
	evl_write_proxy(efd, NULL, 0);
	evl_read_proxy(efd, NULL, 0);
	do_vprint(efd, "%s,%d", "string", 42);
	evl_printf("%s,%d", "string", 42);

	return 0;
}
