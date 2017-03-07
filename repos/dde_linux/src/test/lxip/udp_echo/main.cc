/*
 * \brief  Minimal datagram server demonstration using socket API
 * \author Josef Soentgen
 * \date   2016-04-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <libc/component.h>

/* libc includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>

using namespace Genode;


struct Test_failed : Genode::Exception { };


static void server_loop(Genode::Xml_node config_node)
{
	int s;

	Genode::log("Create new socket ...");
	if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		Genode::error("no socket available!");
		throw Test_failed();
	}

	unsigned port = 0;
	try { config_node.attribute("port").value(&port); }
	catch (...) {
		error("Missing \"port\" attribute.");
		throw Xml_node::Nonexistent_attribute();
	}
	Genode::log("Now, I will bind ...");
	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons(port);
	in_addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr))) {
		Genode::error("bind failed!");
		throw Test_failed();
	}

	Genode::log("Start the server loop ...");
	while(true) {
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		socklen_t len = sizeof(addr);

		char buf[4096];
		::memset(buf, 0, sizeof(buf));

		ssize_t n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &len);

		if (n == 0) {
			Genode::warning("Invalid request!");
			continue;
		}

		if (n < 0) {
			Genode::error("Error ", n);
			break;
		}

		Genode::log("Received ", n, " bytes");
		n = sendto(s, buf, n, 0, (struct sockaddr*)&addr, len);
		Genode::log("Send ", n, " bytes back");
	}
}


struct Main
{
	Main(Genode::Env &env)
	{
		Genode::Attached_rom_dataspace config_rom { env, "config" };

		Libc::with_libc([&] () {
			server_loop(config_rom.xml());
		});
	}
};


void Libc::Component::construct(Libc::Env &env) { static Main main(env); }
