/*
 * \brief  Simple UDP test client
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
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <libc/component.h>
#include <nic/packet_allocator.h>
#include <util/string.h>

/* libc includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace Genode;
using Ipv4_addr_str = Genode::String<16>;

static void test(Libc::Env &env)
{
	Timer::Connection timer(env);

	/* try to send and receive a message multiple times */
	for (unsigned trial_cnt = 0, success_cnt = 0; trial_cnt < 15; trial_cnt++)
	{
		usleep(1000);

		/* create socket */
		int s = socket(AF_INET, SOCK_DGRAM, 0 );
		if (s < 0) {
			continue;
		}
		/* read server IP address and port */
		Attached_rom_dataspace config(env, "config");
		Xml_node config_node = config.xml();

		auto check_attr = [&] (char const *attr)
		{
			if (config_node.has_attribute(attr))
				return true;

			error("cannot read attribute '", attr, "'");
			return false;
		};

		if (!check_attr("server_ip") || !check_attr("server_port"))
			break;

		Ipv4_addr_str const server_addr =
			config_node.attribute_value("server_ip", Ipv4_addr_str());

		unsigned const port =
			config_node.attribute_value("server_port", 0U);

		/* create server socket address */
		struct sockaddr_in addr;
		socklen_t addr_sz = sizeof(addr);
		addr.sin_port = htons(port);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(server_addr.string());

		/* send test message */
		enum { BUF_SZ = 1024 };
		String<BUF_SZ> const message("UDP server at ", server_addr, ":", port);
		if (sendto(s, message.string(), BUF_SZ, 0, (struct sockaddr*)&addr, addr_sz) != BUF_SZ) {
			continue;
		}

		/* receive and print what has been received */
		char buf[BUF_SZ] { };
		if (recvfrom(s, buf, BUF_SZ, 0, (struct sockaddr*)&addr, &addr_sz) != BUF_SZ) {
			continue;
		}
		log("Received \"", String<64>(buf), " ...\"");
		if (++success_cnt >= 5) {
			log("Test done");
			env.parent().exit(0);
			return;
		}
	}
	log("Test failed");
	env.parent().exit(-1);
}

void Libc::Component::construct(Libc::Env &env) { with_libc([&] () { test(env); }); }
