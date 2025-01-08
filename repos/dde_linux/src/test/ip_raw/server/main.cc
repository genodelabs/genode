/*
 * \brief  Genode socket-interface test server part
 * \author Sebastian Sumpf
 * \date   2023-09-21
 */

/*
 * Copyright (C) 2023-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <net/ipv4.h>
#include <util/endian.h>

#include <genode_c_api/socket_types.h>
#include <genode_c_api/socket.h>

#include <data.h>

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

	uint16_t const port { config.xml().attribute_value("port", (uint16_t)80) };

	using Ipv4_string = String<16>;

	Ipv4_string const ip {
		config.xml().attribute_value("ip_addr", Ipv4_string("0.0.0.0")) };
	Ipv4_string const netmask {
		config.xml().attribute_value("netmask", Ipv4_string("0.0.0.0")) };
	Ipv4_string const gateway {
		config.xml().attribute_value("gateway", Ipv4_string("0.0.0.0")) };
	Ipv4_string const nameserver {
		config.xml().attribute_value("nameserver", Ipv4_string("0.0.0.0")) };

	Ipv4_address const ip_addr {
		config.xml().attribute_value("ip_addr", Ipv4_address()) };

	Http http { };
	Data data { };

	enum { SIZE = Data::SIZE };
	char buf[SIZE];

	Server(Env &env) : env(env)
	{
		genode_socket_init(genode_env_ptr(env), nullptr);

		genode_socket_config address_config = {
			.dhcp       = false,
			.ip_addr    = ip.string(),
			.netmask    = netmask.string(),
			.gateway    = gateway.string(),
			.nameserver = nameserver.string(),
		};

		genode_socket_config_address(&address_config);
	}

	void serve(genode_socket_handle *handle)
	{
		Constructible<Msg_header> msg;
		msg.construct(buf, SIZE);

		/* Read the data from the port, blocking if nothing yet there.
		   We assume the request (the part we care about) is in one packet */
		unsigned long bytes = 0;
		Errno err;
		while (true) {
			err = genode_socket_recvmsg(handle, msg->header(), &bytes, false);
			if (err == GENODE_EAGAIN)
				genode_socket_wait_for_progress();
			else break;
		}

		ASSERT("recvmsg...", (bytes == 39 && err == GENODE_ENONE));

		msg.destruct();

		/* Is this an HTTP GET command? (only check the first 5 chars, since
		   there are other formats for GET, and we're keeping it very simple)*/
		ASSERT("message is GET command...", !strcmp(buf, "GET /", 5));

		/* send http header */
		msg.construct(http.header.string(), http.header.length());
		ASSERT("send HTTP header...",
		       genode_socket_sendmsg(handle, msg->header(), &bytes) == GENODE_ENONE
		       && bytes == http.header.length());
		msg.destruct();

		/* Send http header */
		msg.construct(http.html.string(), http.html.length());
		ASSERT("send HTML...",
		       genode_socket_sendmsg(handle, msg->header(), &bytes) == GENODE_ENONE
		       && bytes == http.html.length());
	}

	void run_tcp()
	{
		enum Errno err;
		genode_socket_handle *handle = nullptr;
		genode_socket_handle *handle_reuse = nullptr;
		ASSERT("create new socket (TCP)...",
		       (handle = genode_socket(AF_INET, SOCK_STREAM, 0, &err)) != nullptr);

		ASSERT("create new socket (TCP re-use port)...",
		       (handle_reuse = genode_socket(AF_INET, SOCK_STREAM, 0, &err)) != nullptr);

		int opt = 1;
		ASSERT("setsockopt REUSEPORT handle...",
		       genode_socket_setsockopt(handle, GENODE_SOL_SOCKET, GENODE_SO_REUSEPORT,
		                               &opt, sizeof(opt)) == GENODE_ENONE);
		ASSERT("setsockopt REUSEPORT handle re-use...",
		       genode_socket_setsockopt(handle_reuse, GENODE_SOL_SOCKET, GENODE_SO_REUSEPORT,
		                               &opt, sizeof(opt)) == GENODE_ENONE);

		genode_sockaddr addr;
		addr.family  = AF_INET;
		addr.in.port = host_to_big_endian(port);
		addr.in.addr = INADDR_ANY;
		ASSERT("bind socket...", genode_socket_bind(handle, &addr) == GENODE_ENONE);
		ASSERT("bind socket re-use...", genode_socket_bind(handle_reuse, &addr) == GENODE_ENONE);

		ASSERT("listen...", genode_socket_listen(handle, 5) == GENODE_ENONE);

		genode_socket_handle *client = nullptr;

		err = GENODE_EAGAIN;
		for (unsigned i = 0; i < 100 && err == GENODE_EAGAIN; i++) {
			client = genode_socket_accept(handle, &addr, &err);

			if (err == GENODE_EAGAIN)
				genode_socket_wait_for_progress();
		}

		ASSERT("accept...", err == GENODE_ENONE && client != nullptr);

		serve(client);

		ASSERT("release socket...", genode_socket_release(handle) == GENODE_ENONE);
	}

	void run_udp()
	{
		enum Errno err;
		genode_socket_handle *handle = nullptr;
		ASSERT("create new socket (UDP)...",
		       (handle = genode_socket(AF_INET, SOCK_DGRAM, 0, &err)) != nullptr);

		genode_sockaddr addr;
		addr.family  = AF_INET;
		addr.in.port = host_to_big_endian<genode_uint16_t>(port);
		addr.in.addr = ip_addr.to_uint32_big_endian();
		ASSERT("bind socket...", genode_socket_bind(handle, &addr) == GENODE_ENONE);
		err = genode_socket_bind(handle, &addr);

		unsigned long bytes_recv = 0;
		bool once = true;
		while (bytes_recv < SIZE) {
			unsigned long bytes = 0;
			genode_sockaddr recv_addr = { .family = AF_INET };
			Msg_header msg { recv_addr, buf + bytes_recv, SIZE - bytes_recv };


			err = genode_socket_recvmsg(handle, msg.header(), &bytes, false);

			bytes_recv += bytes;

			if (err == GENODE_EAGAIN) {
				genode_socket_wait_for_progress();
			} else if (err == GENODE_ENONE) {

				if (once) {
					Ipv4_address sender_ip =
						Ipv4_address::from_uint32_big_endian(recv_addr.in.addr);

					char expected[] = { 10, 0, 2, 2 };
					Ipv4_address expected_ip { expected };

					ASSERT("check expected sender IP address...", expected_ip == sender_ip);
					once = false;
				}
			} else break;
		}
		ASSERT("receive bytes...", err == GENODE_ENONE);
		ASSERT("check bytes...", strcmp(data.buffer(), buf, SIZE) == 0);
	}
};


void Component::construct(Genode::Env &env)
{
	static Test::Server server { env };

	try {
		server.run_tcp();
		server.run_udp();
		log("Success");
	} catch (...) {
		log("Failure");
	}
}
