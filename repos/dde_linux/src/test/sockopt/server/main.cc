/*
 * \brief  Libc-sockopt test
 * \author Sebastian Sumpf
 * \date   2025-09-23
 */

/*
 * Copyright (C) 2025-2026 Genode Labs GmbH
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
	using namespace Net;
	struct Server;
};

using namespace Genode;


#define ASSERT(string, cond) { if (!(cond)) {\
	log("[", ++counter, "] ", string, " [failed]"); \
	error("assertion failed at line ", __LINE__, ": ", #cond); \
	throw -1;\
	} else { log("[", ++counter, "] ", string, " [ok]"); } }


struct Test::Server
{
	Env &env;

	unsigned counter { 0 };

	Attached_rom_dataspace config { env, "config" };

	uint16_t const port { config.node().attribute_value("port", (uint16_t)80) };

	void run_accept_and_wait()
	{
		int fd = socket(AF_INET, SOCK_STREAM, 0);
		ASSERT("create socket ...", fd > 0);

		int err = 0;
		int opt = 1;
		err = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		ASSERT("setsocktopt SO_REUSEADDR ...", err == 0);

		unsigned optlen = sizeof(unsigned); opt = 0;
		err = getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, &optlen);
		ASSERT("getsockopt SO_REUSEADDR ...", err == 0 && opt == 1);

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = INADDR_ANY;

		err = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
		ASSERT("bind ...", err == 0);

		int fd_reuse = socket(AF_INET, SOCK_STREAM, 0);
		ASSERT("create socket (re-use fd)...", fd > 0);

		err = bind(fd_reuse, (struct sockaddr*)&addr, sizeof(addr));
		ASSERT("bind re-use fd (should fail) ...", errno == EADDRINUSE);

		err = setsockopt(fd_reuse, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		ASSERT("setsocktopt SO_REUSEADDR (re-use fd) ...", err == 0);

		/*
		 * This will work because 'listen' will be called with fd and INADDR_ANY
		 * later, otherwise this would fail since both sockets use INADDR_ANY
		 * Reference: 'man 7 socket' on Linux
		 */
		err = bind(fd_reuse, (struct sockaddr*)&addr, sizeof(addr));
		ASSERT("bind re-use fd (fails if REUSEADDR is not working) ...", err == 0);

		err = listen(fd_reuse, 5);
		ASSERT("listen ...", err == 0);

		struct sockaddr client_addr;
		socklen_t len = sizeof(client_addr);
		err = accept(fd_reuse, &client_addr, &len);
		ASSERT("accept ...", err > 0);

		sleep(3600);

		close(fd_reuse);
		close(fd);
	}
};


void Libc::Component::construct(Libc::Env &env)
{
	static Test::Server server { env };

	try {
		with_libc([&] () {
			server.run_accept_and_wait();
		});
		log("Success");
	} catch (...) {
		log("Failure");
	}
}
