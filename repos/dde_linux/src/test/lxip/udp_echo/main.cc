/*
 * \brief  Minimal datagram server demonstration using socket API
 * \author Josef Soentgen
 * \date   2016-04-22
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>

/* libc includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <os/config.h>

using namespace Genode;

int main(void)
{
	int s;

	Genode::log("Create new socket ...");
	if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		Genode::error("no socket available!");
		return -1;
	}

	unsigned port = 0;
	Xml_node libc_node = config()->xml_node().sub_node("libc");
	try { libc_node.attribute("port").value(&port); }
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
		return -1;
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
	return 0;
}
