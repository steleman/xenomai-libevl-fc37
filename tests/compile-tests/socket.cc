/*
 * SPDX-License-Identifier: MIT
 *
 * COMPILE-TESTING ONLY.
 */

#include <time.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <netpacket/packet.h>
#include <evl/atomic.h>
#include <evl/syscall.h>
#include <evl/syscall-evl.h>
#include <evl/net/socket.h>
#include <evl/net/socket-evl.h>

int main(int argc, char *argv[])
{
	struct timespec timeout = { 0 };
	struct oob_msghdr msghdr;

	oob_recvmsg(-1, &msghdr, &timeout, 0);
	oob_sendmsg(-1, &msghdr, &timeout, 0);

	return 0;
}
