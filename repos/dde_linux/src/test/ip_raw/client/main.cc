/*
 * \brief  Genode socket-interface test client part
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
	struct Client;
	using namespace Net;
}

using namespace Genode;

#define ASSERT(string, cond) { if (!(cond)) {\
	log("[", ++counter, "] ", string, " [failed]"); \
	error("assertion failed at line ", __LINE__, ": ", #cond); \
	throw -1;\
	} else { log("[", ++counter, "] ", string, " [ok]"); } }

extern "C" void wait_for_continue(void);


struct Test::Client
{
	Env &env;

	using Ipv4_string = String<16>;

	Attached_rom_dataspace config { env, "config"};

	uint16_t const port
		{ config.xml().attribute_value("server_port", (uint16_t)80) };

	Ipv4_address ip_addr
		{ config.xml().attribute_value("server_ip", Ipv4_address()) };

	unsigned long counter { 0 };

	Data data { };
	char *recv_buf[Data::SIZE];

	Client(Env &env) : env(env)
	{
		genode_socket_init(genode_env_ptr(env), nullptr);

		genode_socket_config address_config = { .dhcp = true };
		genode_socket_config_address(&address_config);
	}

	/* connect blocking version */
	bool connect(genode_socket_handle *handle, genode_sockaddr *addr)
	{
		enum Errno err;
		/* is non-blocking */
		err = genode_socket_connect(handle, addr);
		if (err == GENODE_ENONE) return true;
		if (err != GENODE_EINPROGRESS) return false;

		/* proceed with protocol described in manpage */
		bool success = false;
		for (unsigned i = 0; i < 100; i++) {
			if(genode_socket_poll(handle) & genode_socket_pollout_set()) {
				success = true; break;
			}
			genode_socket_wait_for_progress();
		}

		if (!success) return false;

		enum Errno socket_err;
		unsigned size = sizeof(enum Errno);
		err = genode_socket_getsockopt(handle, GENODE_SOL_SOCKET, GENODE_SO_ERROR,
		                               &socket_err, &size);

		if (err || socket_err) return false;

		return true;
	}


	unsigned long  receive(genode_socket_handle *handle, char *buf, unsigned long length)
	{
		Msg_header msg_recv { buf, length };
		unsigned long bytes = 0;
		Errno err;
		while (true) {
			err = genode_socket_recvmsg(handle, msg_recv.header(), &bytes, false);
			if (err == GENODE_EAGAIN)
				genode_socket_wait_for_progress();
			else break;
		}
		return bytes;
	}

	void run_tcp()
	{
		enum Errno err;
		genode_socket_handle *handle = nullptr;
		ASSERT("create new socket (TCP)...",
		       (handle = genode_socket(AF_INET, SOCK_STREAM, 0, &err)) != nullptr);

		genode_sockaddr addr;
		addr.family  = AF_INET;
		addr.in.port = host_to_big_endian(port);
		addr.in.addr = ip_addr.to_uint32_big_endian();
		ASSERT("connect...", connect(handle, &addr) == true);

		genode_sockaddr name;
		ASSERT("getsockname... ", genode_socket_getsockname(handle, &name) == GENODE_ENONE);

		char expected_name[] = { 10, 0, 2, 2 };
		ASSERT("check expected sockname IP...",
			Ipv4_address::from_uint32_big_endian(name.in.addr) == Ipv4_address(expected_name));

		ASSERT("getpeername... ", genode_socket_getpeername(handle, &name) == GENODE_ENONE);

		char expected_peer[] = { 10, 0, 2, 3 };
		ASSERT("check expected peername IP...",
			Ipv4_address::from_uint32_big_endian(name.in.addr) == Ipv4_address(expected_peer));

		/* send request */
		String<64> request { "GET / HTTP/1.0\r\nHost: localhost:80\r\n\r\n" };
		Msg_header msg { request.string(), request.length() };
		unsigned long bytes_send;
		ASSERT("send GET request...",
		       genode_socket_sendmsg(handle, msg.header(), &bytes_send) == GENODE_ENONE
		       && bytes_send == request.length());

		char buf[150];
		Http http {};

		/* http header */
		ASSERT("receive HTTP header...", receive(handle, buf, 150) == http.header.length());
		ASSERT("check HTTP header...", !strcmp(http.header.string(), buf,http.header.length()));

		/* html */
		ASSERT("receive HTML...", receive(handle, buf, 150) == http.html.length());
		ASSERT("check HTML...", !strcmp(http.html.string(), buf, http.html.length() == 0));

		ASSERT("shutdown...", genode_socket_shutdown(handle, SHUT_RDWR) == GENODE_ENONE);
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

		for (unsigned i = 0; i < data.size(); i += MAX_UDP_LOAD) {
			Msg_header msg { addr, data.buffer() + i, MAX_UDP_LOAD};
			unsigned long bytes_send = 0;
			ASSERT("send bytes...",
			       genode_socket_sendmsg(handle, msg.header(), &bytes_send) == GENODE_ENONE
			       && bytes_send == MAX_UDP_LOAD);
		}
	}
};


void Component::construct(Genode::Env &env)
{
	static Test::Client client { env };

	try {
		client.run_tcp();
		client.run_udp();
		log("Success");
	} catch (...) {
		log("Failure");
	}
}
