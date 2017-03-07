/*
 * \brief  Minimal datagram client demonstration using socket API
 * \author Martin Stein
 * \date   2016-08-17
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/snprintf.h>
#include <libc/component.h>
#include <timer_session/connection.h>

/* libc includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>

using namespace Genode;


struct Failure : Genode::Exception { };


static void client_loop(Genode::Xml_node config_node, Timer::Connection &timer)
{
	int s;
	log("Start the client loop ...");
	for(int j = 0; j != 5; ++j) {
		timer.msleep(2000);
		if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			error("No socket available!");
			throw Failure();
		}
		enum { ADDR_STR_SZ = 16 };
		char serv_addr[ADDR_STR_SZ] = { 0 };
		try { config_node.attribute("server_ip").value(serv_addr, ADDR_STR_SZ); }
		catch(...) {
			error("Missing \"server_ip\" attribute.");
			throw Xml_node::Nonexistent_attribute();
		}
		unsigned port = 0;
		try { config_node.attribute("server_port").value(&port); }
		catch (...) {
			error("Missing \"server_port\" attribute.");
			throw Xml_node::Nonexistent_attribute();
		}
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		socklen_t len = sizeof(addr);
		addr.sin_port = htons(port);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(serv_addr);
		char buf[1024];
		Genode::snprintf(buf, sizeof(buf), "UDP server at %s:%u", serv_addr, port);
		ssize_t n = sizeof(buf);
		n = sendto(s, buf, n, 0, (struct sockaddr*)&addr, len);
		n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &len);
		if (n == 0) {
			warning("Invalid reply!");
			continue;
		}
		if (n < 0) {
			error("Error ", n);
			break;
		}
		log("Received \"", String<64>(buf), " ...\"");
	}
	timer.msleep(2000);

	log("Test done");
}


struct Main
{
	Main(Genode::Env &env)
	{
		Genode::Attached_rom_dataspace config_rom { env, "config" };
		Timer::Connection              timer      { env };

		Libc::with_libc([&] () {
			client_loop(config_rom.xml(), timer);
		});
	}
};


void Libc::Component::construct(Libc::Env &env) { static Main main(env); }
