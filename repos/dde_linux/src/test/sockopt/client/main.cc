/*
 * \brief  Libc-sockopt test
 * \author Sebastian Sumpf
 * \date   2025-09-23
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <libc/component.h>
#include <net/ipv4.h>
#include <util/endian.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

namespace Test {
	struct Client;
	using namespace Net;
}

using namespace Genode;

#define ASSERT(string, cond) { if (!(cond)) {\
	log("[", ++counter, "] ", string, " [failed]"); \
	error("assertion failed at line ", __LINE__, ": ", #cond); \
	throw -1;\
	} else { log("[", ++counter, "] ", string, " [ok]"); } }


struct Test::Client
{
	Env &env;

	using Ipv4_string = String<16>;

	Attached_rom_dataspace config { env, "config"};

	uint16_t const port
		{ config.xml().attribute_value("server_port", (uint16_t)80) };

	Ipv4_string ip_addr
		{ config.xml().attribute_value("server_ip", Ipv4_string("0.0.0.0")) };

	unsigned long counter { 0 };

	void run_keepalive()
	{
		struct sockaddr_in addr;
		addr.sin_port        = htons(port);
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = inet_addr(ip_addr.string());

		int fd = -1; int err = -1;
		for (unsigned retry = 0; retry < 2; retry++) {
			fd = socket(AF_INET, SOCK_STREAM, 0);
			ASSERT("create socket ...", fd > 0);

			err = connect(fd, (sockaddr *)&addr, sizeof(addr));
			if (err == 0) break;
			close(fd);
		}
		ASSERT("connect ...", err == 0);

		int opt = 1;
		err = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
		ASSERT("setsockopt SO_KEEPALIVE ...", err == 0);

		unsigned len = 4; opt = 0;
		err = getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, &len);
		ASSERT("getsockopt SO_KEEPALIVE ...", err == 0);

		/* send keepalive after 5s idle */
		opt = 5;
		err = setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &opt, sizeof(opt));
		ASSERT("setsockopt TCP_KEEPIDLE ...", err == 0);

		/*
		 * send 2 keepalive packets after no response has been received before
		 * shutting down the socket
		 */
		opt = 2;
		err = setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &opt, sizeof(opt));
		ASSERT("setsockopt TCP_KEEPCNT ...", err == 0);

		/*
		 * send next keepalive after 1s when no response has been received
		 */
		opt = 1;
		err = setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &opt, sizeof(opt));
		ASSERT("setsockopt TCP_KEEPINTVL ...", err == 0);

		sleep(3600);

		close(fd);
	}
};


void Libc::Component::construct(Libc::Env &env)
{
	static Test::Client client { env };

	try {
		with_libc([&] () {
			client.run_keepalive();
		});
		log("Success");
	} catch (...) {
		log("Failure");
	}
}
