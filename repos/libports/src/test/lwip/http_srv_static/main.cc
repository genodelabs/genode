/*
 * \brief  Minimal HTTP server lwIP demonstration
 * \author lwIP Team
 * \author Stefan Kalkowski
 * \date   2009-10-23
 *
 * This small example shows how to use the LwIP in Genode directly.
 * If you simply want to use LwIP's socket API, you might use
 * Genode's libc together with its LwIP backend, especially useful
 * when porting legacy code.
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <libc/component.h>
#include <nic/packet_allocator.h>
#include <util/string.h>

/* LwIP includes */
extern "C" {
#include <lwip/sockets.h>
#include <lwip/api.h>
}

#include <lwip_legacy/genode.h>

using Response = Genode::String<1024>;


/**
 * Handle a single client's request.
 *
 * \param conn  socket connected to the client
 */
void http_server_serve(int conn, Response & response)
{
	char    buf[1024];
	ssize_t buflen;

	/* Read the data from the port, blocking if nothing yet there.
	   We assume the request (the part we care about) is in one packet */
	buflen = lwip_recv(conn, buf, 1024, 0);
	Genode::log("Packet received!");

	/* Ignore all receive errors */
	if (buflen > 0) {

		/* Is this an HTTP GET command? (only check the first 5 chars, since
		   there are other formats for GET, and we're keeping it very simple)*/
		if (buflen >= 5 &&
			buf[0] == 'G' &&
			buf[1] == 'E' &&
			buf[2] == 'T' &&
			buf[3] == ' ' &&
			buf[4] == '/' ) {

			Genode::log("Will send response");

			/* Send our HTML page */
			lwip_send(conn, response.string(), response.length(), 0);
		}
	}
}

struct Initialization_failed {};
#define ASSERT(cond, err) \
	if (!cond) { \
		Genode::error(err); \
		throw Initialization_failed(); \
	}

void Libc::Component::construct(Libc::Env & env)
{
	using namespace Genode;
	using Address  = Genode::String<16>;

	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };


	lwip_tcpip_init();

	uint32_t ip = 0;
	uint32_t nm = 0;
	uint32_t gw = 0;
	unsigned port = 0;

	Address ip_addr_str;
	Address netmask_str;
	Address gateway_str;

	Attached_rom_dataspace config(env, "config");
	Xml_node libc_node = env.libc_config();
	libc_node.attribute("ip_addr").value(&ip_addr_str);
	libc_node.attribute("netmask").value(&netmask_str);
	libc_node.attribute("gateway").value(&gateway_str);
	config.xml().attribute("port").value(&port);

	log("static network interface: ip=", ip_addr_str, " nm=", netmask_str, " gw=", gateway_str);

	ip = inet_addr(ip_addr_str.string());
	nm = inet_addr(netmask_str.string());
	gw = inet_addr(gateway_str.string());

	ASSERT((ip != INADDR_NONE && nm != INADDR_NONE && gw != INADDR_NONE),
	       "Invalid network interface config.");

	/* Initialize network stack  */
	ASSERT(!lwip_nic_init(ip, nm, gw, BUF_SIZE, BUF_SIZE),
	       "got no IP address!");

	log("Create new socket ...");

	int s;
	ASSERT(((s = lwip_socket(AF_INET, SOCK_STREAM, 0)) >= 0),
	       "no socket available!");

	static Response response(
		"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"
		"<html><head></head><body>"
		"<h1>HTTP server at %s:%u</h1>"
		"<p>This is a small test page.</body></html>",
		ip_addr_str.string(), port);

	log("Now, I will bind ...");

	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons(port);
	in_addr.sin_addr.s_addr = INADDR_ANY;
	ASSERT(!lwip_bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr)),
	       "bind failed!");

	log("Now, I will listen ...");
	ASSERT(!lwip_listen(s, 5),
	       "listen failed!");

	log("Start the server loop ...");
	while (true) {
		struct sockaddr addr;
		socklen_t len = sizeof(addr);
		int client = lwip_accept(s, &addr, &len);
		if(client < 0) {
			warning("invalid socket from accept!");
			continue;
		}
		http_server_serve(client, response);
		lwip_close(client);
	}
}
