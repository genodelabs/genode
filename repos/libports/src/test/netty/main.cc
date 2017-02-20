/*
 * \brief  Network echo test
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


enum Test_mode { TEST_READER_WRITER, TEST_TRADITIONAL };

static Test_mode const test_mode = TEST_TRADITIONAL;


/***************************************
 ** Multi-threaded reader-writer test **
 ***************************************/

class Worker : public Genode::Thread_deprecated<0x4000>
{
	protected:

		char     _name[128];
		unsigned _id;
		int      _sd;
		char     _buf[4096];

		char const *_init_name(char const *type, unsigned id)
		{
			snprintf(_name, sizeof(_name), "%s.%u", type, id);
			return _name;
		}

	public:

		Worker(char const *type, unsigned id, int sd)
		:
			Genode::Thread_deprecated<0x4000>(_init_name(type, id)),
			_id(id), _sd(sd)
		{ }
};


struct Reader : Worker
{
	Reader(unsigned id, int sd) : Worker("reader", id, sd) { }

	void entry() override
	{
		Timer::Connection timer;

		while (true) {
//			timer.msleep(100);

			int const ret = ::read(_sd, _buf, sizeof(_buf));

			if (ret == -1)
				break;
		}

		Genode::log(Genode::Cstring(_name), " finished errno=", errno);
	}
};


struct Writer : Worker
{
	Writer(unsigned id, int sd) : Worker("writer", id, sd) { }

	void entry() override
	{
		Timer::Connection timer;

		size_t size = 0;

		if (0) {
			size = sizeof(_buf);
			memset(_buf, 'x', size - 1);
		} else {
			size = strlen(_name) + 1;
			strcpy(_buf, _name);
		}
		_buf[size - 1] = '\n';

		while (true) {
//			timer.msleep(10 + _id*5);

			int const ret = ::write(_sd, _buf, size);

			if (ret == -1)
				break;
		}

		Genode::log(Genode::Cstring(_name), " finished errno=", errno);
	}
};


static void test_reader_writer(int cd)
{
	static Reader r0(0, cd);
	static Reader r1(1, cd);
	static Reader r2(2, cd);
	static Writer w0(0, cd);
	static Writer w1(1, cd);
	static Writer w2(2, cd);

	r0.start();
	r1.start();
	r2.start();
	w0.start();
	w1.start();
	w2.start();

	Genode::sleep_forever();
}


/****************************************
 ** Traditional (blocking) socket test **
 ****************************************/

static size_t test_traditional(int cd, bool const use_read_write)
{
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
		if (ret == 0) return count;

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


static void server(Genode::Xml_node const config)
{
	int ret = 0;

	Genode::log("Let's serve");

	int sd = socket(AF_INET, SOCK_STREAM, 0);

	Genode::log("sd=", sd);
	if (sd == -1) DIE("socket");

	unsigned const port           = config.attribute_value("port", 8080U);
	bool     const use_read_write = config.attribute_value("read_write", false);

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

		if (0) {
			test_reader_writer(cd);
		} else {
			size_t const count = test_traditional(cd, use_read_write);
			Genode::log("echoed ", count, " bytes");
		}

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
				server(config);
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
