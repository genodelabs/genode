/*
 * \brief  Simple UDP test server
 * \author Martin Stein
 * \date   2017-10-18
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <libc/component.h>
#include <nic/packet_allocator.h>
#include <util/string.h>

/* libc includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace Genode;
using Ipv4_addr_str = Genode::String<16>;

struct Socket_failed     : Genode::Exception { };
struct Send_failed       : Genode::Exception { };
struct Receive_failed    : Genode::Exception { };
struct Bind_failed       : Genode::Exception { };

struct Read_port_attr_failed : Genode::Exception { };


static void test(Libc::Env & env)
{
	/* create socket */
	int s = socket(AF_INET, SOCK_DGRAM, 0 );
	if (s < 0) {
		throw Socket_failed();
	}
	/* read server port */
	unsigned port = 0;
	env.config([&] (Xml_node config_node) {
		try { config_node.attribute("port").value(&port); }
		catch (...) {
			throw Read_port_attr_failed();
		}
	});
	/* create server socket address */
	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons(port);
	in_addr.sin_addr.s_addr = INADDR_ANY;

	/* bind server socket address to socket */
	if (bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr))) {
		throw Bind_failed();
	}
	/* prepare client socket address */
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	socklen_t addr_sz = sizeof(addr);

	while (true) {

		/* receive and send back one message without any modifications */
		char buf[4096];
		::memset(buf, 0, sizeof(buf));
		ssize_t rcv_sz = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addr_sz);
		if (rcv_sz < 0) {
			throw Receive_failed();
		}
		if (sendto(s, buf, rcv_sz, 0, (struct sockaddr*)&addr, addr_sz) != rcv_sz) {
			throw Send_failed();
		}
	}
}

void Libc::Component::construct(Libc::Env &env) { with_libc([&] () { test(env); }); }
