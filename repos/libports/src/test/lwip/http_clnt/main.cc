/*
 * \brief  HTTP client test
 * \author Ivan Loskutov
 * \author Martin Stein
 * \date   2012-12-21
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <libc/component.h>
#include <nic/packet_allocator.h>
#include <timer_session/connection.h>
#include <util/string.h>

/* Libc includes */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace Genode;

void close_socket(Libc::Env &env, int sd)
{
	if (::shutdown(sd, SHUT_RDWR)) {
		error("failed to shutdown");
		env.parent().exit(-1);
	}
	if (::close(sd)) {
		error("failed to close");
		env.parent().exit(-1);
	}
}


static void test(Libc::Env &env)
{
	using Ipv4_string = String<16>;
	enum { NR_OF_REPLIES = 5 };
	enum { NR_OF_TRIALS  = 15 };

	/* read component configuration */
	Attached_rom_dataspace  config_rom  { env, "config" };
	Xml_node                config_node { config_rom.xml() };
	Ipv4_string       const srv_ip      { config_node.attribute_value("server_ip", Ipv4_string("0.0.0.0")) };
	uint16_t          const srv_port    { config_node.attribute_value("server_port", (uint16_t)0) };

	/* construct server socket address */
	struct sockaddr_in srv_addr;
	srv_addr.sin_port        = htons(srv_port);
	srv_addr.sin_family      = AF_INET;
	srv_addr.sin_addr.s_addr = inet_addr(srv_ip.string());

	/* try several times to request a reply */
	for (unsigned trial_cnt = 0, reply_cnt = 0; trial_cnt < NR_OF_TRIALS;
	     trial_cnt++)
	{
		/* pause a while between each trial */
		usleep(1000000);

		/* create socket */
		int sd = ::socket(AF_INET, SOCK_STREAM, 0);
		if (sd < 0) {
			error("failed to create socket");
			continue;
		}
		/* connect to server */
		if (::connect(sd, (struct sockaddr *)&srv_addr, sizeof(srv_addr))) {
			error("Failed to connect to server");
			close_socket(env, sd);
			continue;
		}
		/* send request */
		char   const *req    = "GET / HTTP/1.0\r\nHost: localhost:80\r\n\r\n";
		size_t const  req_sz = Genode::strlen(req);
		if (::send(sd, req, req_sz, 0) != (int)req_sz) {
			error("failed to send request");
			close_socket(env, sd);
			continue;
		}
		/* receive reply */
		enum { REPLY_BUF_SZ = 1024 };
		char          reply_buf[REPLY_BUF_SZ];
		size_t        reply_sz     = 0;
		bool          reply_failed = false;
		char   const *reply_end    = "</html>";
		size_t const  reply_end_sz = Genode::strlen(reply_end);
		for (; reply_sz <= REPLY_BUF_SZ; ) {
			char         *rcv_buf    = &reply_buf[reply_sz];
			size_t const  rcv_buf_sz = REPLY_BUF_SZ - reply_sz;
			signed long   rcv_sz     = ::recv(sd, rcv_buf, rcv_buf_sz, 0);
			if (rcv_sz < 0) {
				reply_failed = true;
				break;
			}
			reply_sz += rcv_sz;
			if (reply_sz >= reply_end_sz) {
				if (!strcmp(&reply_buf[reply_sz - reply_end_sz], reply_end, reply_end_sz)) {
					break; }
			}
		}
		/* ignore failed replies */
		if (reply_failed) {
			error("failed to receive reply");
			close_socket(env, sd);
			continue;
		}
		/* handle reply */
		reply_buf[reply_sz] = 0;
		log("Received \"", Cstring(reply_buf), "\"");
		if (++reply_cnt == NR_OF_REPLIES) {
			log("Test done");
			env.parent().exit(0);
		}
		/* close socket and retry */
		close_socket(env, sd);
	}
	log("Test failed");
	env.parent().exit(-1);
}

void Libc::Component::construct(Libc::Env &env) { with_libc([&] () { test(env); }); }
