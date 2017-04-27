/*
 * \brief  Network echo test
 * \author Christian Helmuth
 * \date   2017-04-24
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Local includes */
#include <netty.h>


void Netty::Test::nonblocking(int fd)
{
	int ret = 0;

	ret = fcntl(fd, F_GETFL);
	if (ret == -1) DIE("fcntl");
	Genode::log("F_GETFL returned ", Genode::Hex(ret), "(O_NONBLOCK=", !!(ret & O_NONBLOCK), ")");

	ret = fcntl(fd, F_SETFL, ret | O_NONBLOCK);
	if (ret == -1) DIE("fcntl");

	ret = fcntl(fd, F_GETFL);
	if (ret == -1) DIE("fcntl");
	Genode::log("F_GETFL returned ", Genode::Hex(ret), "(O_NONBLOCK=", !!(ret & O_NONBLOCK), ")");
}


void Netty::Test::_server()
{
	int ret = 0;

	Genode::log("initialize server");

	int const sd = socket();

	Genode::log("sd=", sd);
	if (sd == -1) DIE("socket");

	unsigned const port       = _config.attribute_value("port", 8080U);
	bool     const read_write = _config.attribute_value("read_write", false);
	bool     const nonblock   = _config.attribute_value("nonblock", false);

	Genode::log("config: port=", port, " read_write=", read_write,
	            " nonblock=", nonblock);

	sockaddr_in const  addr { 0, AF_INET, htons(port), { INADDR_ANY } };
	sockaddr    const *paddr = reinterpret_cast<sockaddr const *>(&addr);

	if (1) {
		int const on = 1;
		ret = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	}

	ret = bind(sd, paddr, sizeof(addr));
	if (ret == -1) DIE("bind");

	server(sd, nonblock, read_write);

	close(sd);
	if (ret == -1) DIE("close");
}


void Netty::Test::_client()
{
	int ret = 0;

	Genode::log("initialize client");

	int const sd = socket();

	Genode::log("sd=", sd);
	if (sd == -1) DIE("socket");

	String   const ip(_config.attribute_value("ip", String("10.0.2.1")));
	unsigned const port       = _config.attribute_value("port", 8080U);
	bool     const read_write = _config.attribute_value("read_write", false);
	bool     const nonblock   = _config.attribute_value("nonblock", false);

	Genode::log("config: ip=", ip.string(), " port=", port,
	            " read_write=", read_write, " nonblock=", nonblock);

	sockaddr_in const addr { 0, AF_INET, htons(port), { inet_addr(ip.string()) } };

	client(sd, addr, nonblock, read_write);

	ret = close(sd);
	if (ret == -1) DIE("close");

	Genode::log("client test finished");
}


void Netty::Test::getnames(int const sd)
{
	int err = 0;

	sockaddr_in    addr;
	socklen_t addr_len;

	memset(&addr, 0, sizeof(addr));
	addr_len = sizeof(addr);

	err = getsockname(sd, (sockaddr *)&addr, &addr_len);
	if (err == -1) perror("getsockname");

	Genode::log("sock ", addr);

	memset(&addr, 0, sizeof(addr));
	addr_len = sizeof(addr);

	err = getpeername(sd, (sockaddr *)&addr, &addr_len);
	if (err == -1) perror("getpeername");

	Genode::log("peer ", addr);
}


void Netty::Test::run()
{
	String mode { _config.attribute_value("mode", String("server")) };

	Libc::with_libc([&] () {
		if (mode == "server") {
			_server();
		} else if (mode == "client") {
			_client();
		} else {
			Genode::error("unknown mode '", mode.string(), "'");
			exit(__LINE__);
		}
	});
}
