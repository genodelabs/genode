/*
 * \brief  RFC862 echo server
 * \author Emery Hemingway
 * \date   2016-10-17
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* libc includes */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define ECHO_PORT  7
#define MAXBUFLEN  0xFFFF
#define RECV_FLAGS 0
#define SEND_FLAGS 0

static void print(Genode::Output &output, sockaddr_in const &addr)
{
	print(output, (ntohl(addr.sin_addr.s_addr) >> 24) & 0xff);
	output.out_string(".");
	print(output, (ntohl(addr.sin_addr.s_addr) >> 16) & 0xff);
	output.out_string(".");
	print(output, (ntohl(addr.sin_addr.s_addr) >>  8) & 0xff);
	output.out_string(".");
	print(output, (ntohl(addr.sin_addr.s_addr) >>  0) & 0xff);
	output.out_string(":");
	print(output, ntohs(addr.sin_port));
}

int main(void)
{
	int udp_sock;
	int rv = 0;
	int err = 0;
	ssize_t numbytes;
	struct sockaddr_in their_addr;
	char buf[MAXBUFLEN];
	socklen_t addr_len;
	struct sockaddr_in const addr = { 0, AF_INET, htons(ECHO_PORT), { INADDR_ANY } };

	Genode::log("Create, bind, and close test...");
	unsigned i = 0;
	for (; i < 10000; i++) {

		udp_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (udp_sock < 0) {
			Genode::log("create failed with error ", udp_sock);
			return errno;
		}
		err = bind(udp_sock, (struct sockaddr*)&addr, sizeof(addr));
		if (err) {
			Genode::log("bind failed with error ", err);
			return errno;
		}
		err = close(udp_sock);
		if (err) {
			Genode::log("close failed with error ", err);
			return errno;
		}
	}
	Genode::log("Create, bind, and close test succeeded");
	Genode::log("UDP echo test...");

	udp_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udp_sock < 0) {
		Genode::log("create failed with error ", udp_sock);
		return errno;
	}
	err = bind(udp_sock, (struct sockaddr*)&addr, sizeof(addr));
	if (err) {
		Genode::log("bind failed with error ", err);
		return errno;
	}

	for (;;) {
		fd_set read_fds;

		FD_ZERO(&read_fds);
		FD_SET(udp_sock, &read_fds);

		timeval tv { 10, 0 };

		int num_ready = select(udp_sock + 1, &read_fds, nullptr, nullptr, &tv);
		if (num_ready == -1) {
			perror("select failed");
			break;
		}
		if (!num_ready) {
			Genode::log("timeout");
			continue;
		}
		if (!FD_ISSET(udp_sock, &read_fds)) {
			Genode::log("spurious wakeup");
			continue;
		}
		Genode::log("num_ready=", num_ready);

		addr_len = sizeof their_addr;
		numbytes = recvfrom(udp_sock, buf, sizeof(buf), RECV_FLAGS,
		                    (struct sockaddr *)&their_addr, &addr_len);
		if (numbytes == -1) {
			rv = errno;
			perror("recvfrom failed");
			break;
		}

		Genode::log("received ", numbytes, " bytes from ", their_addr);

		numbytes = sendto(udp_sock, buf, numbytes, SEND_FLAGS,
		                   (struct sockaddr *)&their_addr, addr_len);
		if (numbytes == -1) {
			rv = errno;
			perror("sendto failed");
			break;
		}

		Genode::log("sent ", numbytes, " bytes to ", their_addr);
	}

	close(udp_sock);
	return rv;
}
