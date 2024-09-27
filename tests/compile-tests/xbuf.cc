/*
 * SPDX-License-Identifier: MIT
 *
 * COMPILE-TESTING ONLY.
 */

#include <evl/xbuf.h>
#include <evl/xbuf-evl.h>

int main(int argc, char *argv[])
{
	evl_new_xbuf(16384, "test-xbuf");

	return 0;
}
