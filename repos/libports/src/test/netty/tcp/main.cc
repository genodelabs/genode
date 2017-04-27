/*
 * \brief  Network TCP echo test
 * \author Christian Helmuth
 * \date   2017-04-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Local includes */
#include <netty.h>


namespace Netty { struct Tcp; }


struct Netty::Tcp : Netty::Test
{
	Tcp(Libc::Env &env) : Test(env) { run(); }

	int socket() override;
	void server(int, bool, bool) override;
	void client(int, sockaddr_in, bool, bool) override;
};


int Netty::Tcp::socket() { return ::socket(AF_INET, SOCK_STREAM, 0); }


void Netty::Tcp::server(int const sd, bool const nonblock, bool const read_write)
{
	int ret = 0;

	ret = listen(sd, SOMAXCONN);
	if (ret == -1) DIE("listen");

	while (true) {
		sockaddr_in  caddr;
		socklen_t    scaddr = sizeof(caddr);
		sockaddr    *pcaddr = reinterpret_cast<sockaddr *>(&caddr);

		if (nonblock) {
			nonblocking(sd);

			Genode::log("I want EAGAIN");
			int test = accept(sd, pcaddr, &scaddr);
			if (test == -1 && errno == EAGAIN)
				Genode::log("I got EAGAIN");
			else
				Genode::error("Did not get EAGAIN but test=", test, " errno=", errno);

			fd_set read_fds; FD_ZERO(&read_fds); FD_SET(sd, &read_fds);
			ret = select(sd + 1, &read_fds, nullptr, nullptr, nullptr);
			if (ret == -1) DIE("select");
			Genode::log("okay, accept will not block");
		}

		Genode::log("test in ", nonblock ? "non-blocking" : "blocking", " mode");

		int const cd = accept(sd, pcaddr, &scaddr);
		Genode::log("cd=", cd);
		if (cd == -1) DIE("accept");

		getnames(cd);

		size_t count = 0;
		static char data[64*1024];

		if (nonblock) nonblocking(cd);

		while (true) {
			int ret = read_write
			        ? read(cd, data, sizeof(data))
			        : recv(cd, data, sizeof(data), 0);

			if (ret == 0) {
				Genode::log("experienced EOF");
				break;
			}

			if (ret > 0) {
				/* echo received data */
				ret = read_write
				    ? write(cd, data, ret)
				    : send(cd, data, ret, 0);
				if (ret == -1) DIE(read_write ? "write" : "send");

				count += ret;
				continue;
			}

			if (!nonblock || errno != EAGAIN)
				DIE(read_write ? "read" : "recv");

			Genode::log("block in select because of EAGAIN");
			fd_set read_fds; FD_ZERO(&read_fds); FD_SET(cd, &read_fds);
			ret = select(cd + 1, &read_fds, nullptr, nullptr, nullptr);
			if (ret == -1) DIE("select");
		}

		Genode::log("echoed ", count, " bytes");

		ret = shutdown(cd, SHUT_RDWR);
		if (ret == -1) DIE("shutdown");

		ret = close(cd);
		if (ret == -1) DIE("close");
	}
}


void Netty::Tcp::client(int const sd, sockaddr_in const addr,
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


void Libc::Component::construct(Libc::Env &env) { static Netty::Tcp inst(env); }
