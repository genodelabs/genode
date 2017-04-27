/*
 * \brief  Network UDP echo test
 * \author Christian Helmuth
 * \date   2017-04-25
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Local includes */
#include <netty.h>


namespace Netty { struct Udp; }


struct Netty::Udp : Netty::Test
{
	Udp(Libc::Env &env) : Test(env) { run(); }

	int socket() override;
	void server(int, bool, bool) override;
	void client(int, sockaddr_in, bool, bool) override;
};


int Netty::Udp::socket() { return ::socket(AF_INET, SOCK_DGRAM, 0); }


void Netty::Udp::server(int const sd, bool const nonblock, bool const read_write)
{
	if (read_write)
		Genode::warning("ignoring read_write attribute for UDP tests");

	int ret = 0;

	while (true) {
		sockaddr_in  caddr;
		socklen_t    scaddr = sizeof(caddr);
		sockaddr    *pcaddr = reinterpret_cast<sockaddr *>(&caddr);

		static char data[64*1024];

		if (nonblock) {
			nonblocking(sd);

			Genode::log("I want EAGAIN");
			ssize_t test = recvfrom(sd, data, sizeof(data), 0, pcaddr, &scaddr);
			if (test == -1 && errno == EAGAIN)
				Genode::log("I got EAGAIN");
			else
				Genode::error("Did not get EAGAIN but test=", test, " errno=", errno);

			fd_set read_fds; FD_ZERO(&read_fds); FD_SET(sd, &read_fds);
			ret = select(sd + 1, &read_fds, nullptr, nullptr, nullptr);
			if (ret == -1) DIE("select");
			Genode::log("okay, recvfrom will not block");
		}

		Genode::log("test in ", nonblock ? "non-blocking" : "blocking", " mode");

		ssize_t count = recvfrom(sd, data, sizeof(data), 0, pcaddr, &scaddr);
		if (count == -1) DIE("recvfrom");

		{
			sockaddr_in addr;
			socklen_t   addr_len = sizeof(addr);

			getsockname(sd, (sockaddr *)&addr, &addr_len);
			Genode::log("sock ", addr);
			Genode::log("peer ", caddr);
		}

		count = sendto(sd, data, count, 0, pcaddr, scaddr);

		Genode::log("echoed ", count, " bytes");
	}
}


void Netty::Udp::client(int const sd, sockaddr_in const addr,
                        bool const nonblock, bool const read_write)
{
	/* TODO nonblock, read_write */

	sockaddr const *paddr = reinterpret_cast<sockaddr const *>(&addr);

	int ret = 0;

	ret = connect(sd, paddr, sizeof(addr));
	if (ret == -1) DIE("connect");

	Genode::log("connected");

	static char data[16*1024];
	memset(data, 'X', sizeof(data));

	ret = send(sd, data, sizeof(data), 0);
	if (ret == -1) DIE("send");

	ret = shutdown(sd, SHUT_RDWR);
	if (ret == -1) DIE("shutdown");
}


void Libc::Component::construct(Libc::Env &env) { static Netty::Udp inst(env); }
