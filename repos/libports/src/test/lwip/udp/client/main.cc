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

using namespace Genode;
using Ipv4_addr_str = Genode::String<16>;

struct Socket_failed  : Genode::Exception { };
struct Send_failed    : Genode::Exception { };
struct Receive_failed : Genode::Exception { };

struct Read_server_port_attr_failed : Exception { };
struct Read_server_ip_attr_failed   : Exception { };


void Libc::Component::construct(Libc::Env &env)
{
	/* wait a while for the server to come up */
	Timer::Connection timer(env);
	timer.msleep(6000);

	/* create socket */
	int s = socket(AF_INET, SOCK_DGRAM, 0 );
	if (s < 0) {
		throw Socket_failed();
	}
	/* try to send and receive a message multiple times */
	for (int j = 0; j != 5; ++j) {
		timer.msleep(1000);

		/* read server IP address and port */
		Ipv4_addr_str serv_addr;
		unsigned port = 0;
		Attached_rom_dataspace config(env, "config");
		Xml_node config_node = config.xml();
		try { config_node.attribute("server_ip").value(&serv_addr); }
		catch (...) {
			throw Read_server_port_attr_failed();
		}
		try { config_node.attribute("server_port").value(&port); }
		catch (...) {
			throw Read_server_port_attr_failed();
		}
		/* create server socket address */
		struct sockaddr_in addr;
		socklen_t addr_sz = sizeof(addr);
		addr.sin_port = htons(port);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(serv_addr.string());

		/* send test message */
		enum { BUF_SZ = 1024 };
		char buf[BUF_SZ];
		::snprintf(buf, BUF_SZ, "UDP server at %s:%u", serv_addr.string(), port);
		if (sendto(s, buf, BUF_SZ, 0, (struct sockaddr*)&addr, addr_sz) != BUF_SZ) {
			throw Send_failed();
		}
		/* receive and print what has been received */
		if (recvfrom(s, buf, BUF_SZ, 0, (struct sockaddr*)&addr, &addr_sz) != BUF_SZ) {
			throw Receive_failed();
		}
		log("Received \"", String<64>(buf), " ...\"");
	}
	log("Test done");
}
