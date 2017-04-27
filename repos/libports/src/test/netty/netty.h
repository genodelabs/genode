/*
 * \brief  Network echo test utilities
 * \author Christian Helmuth
 * \date   2017-04-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NETTY_H_
#define _NETTY_H_

/* Genode includes */
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <libc/component.h>
#include <timer_session/connection.h>

/* Libc includes */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define DIE(step)                  \
	do {                           \
		Genode::error("dying..."); \
		perror(step);              \
		exit(1);                   \
	} while (0)


static inline void print(Genode::Output &output, sockaddr_in const &addr)
{
	Genode::print(output, (ntohl(addr.sin_addr.s_addr) >> 24) & 0xff);
	output.out_string(".");
	Genode::print(output, (ntohl(addr.sin_addr.s_addr) >> 16) & 0xff);
	output.out_string(".");
	Genode::print(output, (ntohl(addr.sin_addr.s_addr) >>  8) & 0xff);
	output.out_string(".");
	Genode::print(output, (ntohl(addr.sin_addr.s_addr) >>  0) & 0xff);
	output.out_string(":");
	Genode::print(output, ntohs(addr.sin_port));
}


namespace Netty {

	typedef Genode::String<32> String;

	struct Test;
}


struct Netty::Test
{
	private:

		Libc::Env                      &_env;
		Genode::Attached_rom_dataspace  _config_rom { _env, "config" };
		Genode::Xml_node                _config     { _config_rom.xml() };

		void _server();
		void _client();

	public:

		Test(Libc::Env &env) : _env(env) { }

		virtual ~Test() { }

		void run();
		void getnames(int const sd);
		void nonblocking(int fd);

		/* hooks for the test */
		virtual int socket() = 0;
		virtual void server(int sd, bool nonblock, bool read_write) = 0;
		virtual void client(int sd, sockaddr_in addr, bool nonblock, bool read_write) = 0;
};

#endif /* _NETTY_H_ */
