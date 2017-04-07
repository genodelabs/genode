/*
 * \brief  Network TCP echo test
 * \author Christian Helmuth
 * \date   2015-09-21
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <base/attached_rom_dataspace.h>
#include <base/sleep.h>
#include <libc/component.h>
#include <timer_session/connection.h>

/* Libc includes */
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


typedef Genode::String<32> String;


#define DIE(step)                  \
	do {                           \
		Genode::error("dying..."); \
		perror(step);              \
		exit(1);                   \
	} while (0)


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


static size_t test_nonblocking(int cd, bool const use_read_write)
{
	Genode::log("test in non-blocking mode");

	int ret = 0;

	{
		ret = fcntl(cd, F_GETFL);
		if (ret == -1) DIE("fcntl");
		Genode::log("F_GETFL returned ", Genode::Hex(ret), "(O_NONBLOCK=", !!(ret & O_NONBLOCK), ")");

		ret = fcntl(cd, F_SETFL, ret | O_NONBLOCK);
		if (ret == -1) DIE("fcntl");

		ret = fcntl(cd, F_GETFL);
		if (ret == -1) DIE("fcntl");
		Genode::log("F_GETFL returned ", Genode::Hex(ret), "(O_NONBLOCK=", !!(ret & O_NONBLOCK), ")");
	}

	size_t count = 0;
	static char data[64*1024];

	while (true) {
		ret = use_read_write
		    ? read(cd, data, sizeof(data))
		    : recv(cd, data, sizeof(data), 0);

		if (ret == 0) {
			Genode::log("experienced EOF");
			return count;
		}

		if (ret > 0) {
			/* echo received data */
			ret = use_read_write
			    ? write(cd, data, ret)
			    : send(cd, data, ret, 0);
			if (ret == -1) DIE(use_read_write ? "write" : "send");

			count += ret;
			continue;
		}

		if (errno != EAGAIN) DIE(use_read_write ? "read" : "recv");

		Genode::log("block in select because of EAGAIN");
		fd_set read_fds; FD_ZERO(&read_fds); FD_SET(cd, &read_fds);
		int ret = select(cd + 1, &read_fds, nullptr, nullptr, nullptr);
		if (ret == -1) DIE("select");
	}
}


static size_t test_blocking(int cd, bool const use_read_write)
{
	Genode::log("test in blocking mode");

	size_t count = 0;
	int ret = 0;
	static char data[64*1024];

	while (true) {
		if (use_read_write) {
			ret = read(cd, data, sizeof(data));
			if (ret == -1) DIE("read");
		} else {
			ret = recv(cd, data, sizeof(data), 0);
			if (ret == -1) DIE("recv");
		}

		/* EOF */
		if (ret == 0) {
			Genode::log("experienced EOF");
			return count;
		}

		if (use_read_write) {
			ret = write(cd, data, ret);
			if (ret == -1) DIE("write");
		} else {
			ret = send(cd, data, ret, 0);
			if (ret == -1) DIE("send");
		}

		count += ret;
	}
}


static void test_getnames(int sd)
{
	int err = 0;

	sockaddr_in    addr;
	socklen_t addr_len;

	memset(&addr, 0, sizeof(addr));
	addr_len = sizeof(addr);

	err = getsockname(sd, (sockaddr *)&addr, &addr_len);
	if (err == -1) DIE("getsockname");

	Genode::log("sock ", addr);

	memset(&addr, 0, sizeof(addr));
	addr_len = sizeof(addr);

	err = getpeername(sd, (sockaddr *)&addr, &addr_len);
	if (err == -1) DIE("getpeername");

	Genode::log("peer ", addr);
}


static void server(Genode::Env &env, Genode::Xml_node const config)
{
	int ret = 0;

	Genode::log("Let's serve");

	int sd = socket(AF_INET, SOCK_STREAM, 0);

	Genode::log("sd=", sd);
	if (sd == -1) DIE("socket");

	unsigned const port           = config.attribute_value("port", 8080U);
	bool     const use_read_write = config.attribute_value("read_write", false);
	bool     const nonblock       = config.attribute_value("nonblock", false);

	Genode::log("config: port=", port, " read_write=", use_read_write,
	            " nonblock=", nonblock);

	sockaddr_in const  addr { 0, AF_INET, htons(port), { INADDR_ANY } };
	sockaddr    const *paddr = reinterpret_cast<sockaddr const *>(&addr);

	if (1) {
		int const on = 1;
		ret = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	}

	ret = bind(sd, paddr, sizeof(addr));
	if (ret == -1) DIE("bind");

	ret = listen(sd, SOMAXCONN);
	if (ret == -1) DIE("listen");

	bool const loop = true;
	do {
		Genode::log("accepting connections on ", port);

		sockaddr_in  caddr;
		socklen_t    scaddr = sizeof(caddr);
		sockaddr    *pcaddr = reinterpret_cast<sockaddr *>(&caddr);
		int cd = accept(sd, pcaddr, &scaddr);
		Genode::log("cd=", cd);
		if (cd == -1) DIE("accept");

		test_getnames(cd);

		size_t const count = nonblock
		                   ? test_nonblocking(cd, use_read_write)
		                   : test_blocking(cd, use_read_write);
		Genode::log("echoed ", count, " bytes");

		ret = shutdown(cd, SHUT_RDWR);
		if (ret == -1) DIE("shutdown");

		ret = close(cd);
		if (ret == -1) DIE("close");

	} while (loop);
}


static void client(Genode::Xml_node const config)
{
	int ret = 0;

	Genode::log("Let's connect");

	int sd = socket(AF_INET, SOCK_STREAM, 0);

	Genode::log("sd=", sd);
	if (sd == -1) DIE("socket");

	String   const ip(config.attribute_value("ip", String("10.0.2.1")));
	unsigned const port(config.attribute_value("port", 8080U));

	Genode::log("Connecting to %s:%u", ip.string(), port);

	sockaddr_in const  addr { 0, AF_INET, htons(port), { inet_addr(ip.string()) } };
	sockaddr    const *paddr = reinterpret_cast<sockaddr const *>(&addr);

	ret = connect(sd, paddr, sizeof(addr));
	if (ret == -1) DIE("connect");

	Genode::log("connected");

	static char data[1*1024*1024];
	memset(data, 'X', sizeof(data));

	/* wait for go */
	char go;
	ret = recv(sd, &go, sizeof(go), 0);
	if (ret == -1) DIE("recv");

	/* EOF */
	if (ret == 0) DIE("EOF");

	ret = send(sd, data, sizeof(data), 0);
	if (ret == -1) DIE("send");

	ret = shutdown(sd, SHUT_RDWR);
	if (ret == -1) DIE("shutdown");

	ret = close(sd);
	if (ret == -1) DIE("close");
}


struct Main
{
	String mode { "server" };

	Main(Genode::Env &env)
	{
		Libc::with_libc([&] () {
			Genode::Xml_node config { "<empty/>" };

			/* parse mode configuration */
			try {
				static Genode::Attached_rom_dataspace rom(env, "config");

				config = rom.xml();
			} catch (Genode::Rom_connection::Rom_connection_failed) { }

			mode = config.attribute_value("mode", mode);

			if (mode == "server") {
				server(env, config);
			} else if (mode == "client") {
				client(config);
			} else {
				Genode::error("unknown mode '", mode.string(), "'");
				exit(__LINE__);
			}
		});
	}
};


void Libc::Component::construct(Libc::Env &env) { static Main inst(env); }
