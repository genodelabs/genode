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
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <util/string.h>
#include <nic/packet_allocator.h>
#include <os/config.h>
#include <base/snprintf.h>

/* LwIP includes */
extern "C" {
#include <lwip/sockets.h>
#include <lwip/api.h>
}

#include <lwip/genode.h>


const static char http_html_hdr[] =
	"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"; /* HTTP response header */

enum { HTTP_INDEX_HTML_SZ = 1024 };

static char http_index_html[HTTP_INDEX_HTML_SZ]; /* HTML page */

using namespace Genode;


/**
 * Handle a single client's request.
 *
 * \param conn  socket connected to the client
 */
void http_server_serve(int conn)
{
	char    buf[1024];
	ssize_t buflen;

	/* Read the data from the port, blocking if nothing yet there.
	   We assume the request (the part we care about) is in one packet */
	buflen = lwip_recv(conn, buf, 1024, 0);
	log("Packet received!");

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

			log("Will send response");

			/* Send http header */
			lwip_send(conn, http_html_hdr, Genode::strlen(http_html_hdr), 0);

			/* Send our HTML page */
			lwip_send(conn, http_index_html, Genode::strlen(http_index_html), 0);
		}
	}
}


template <size_t N>
static Genode::String<N> read_string_attribute(Genode::Xml_node node, char const *attr,
                                               Genode::String<N> default_value)
{
	try {
		char buf[N];
		node.attribute(attr).value(buf, sizeof(buf));
		return Genode::String<N>(Genode::Cstring(buf));
	}
	catch (...) {
		return default_value; }
}


int main()
{
	using namespace Genode;

	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	int s;

	lwip_tcpip_init();

	enum { ADDR_STR_SZ = 16 };

	uint32_t ip = 0;
	uint32_t nm = 0;
	uint32_t gw = 0;
	unsigned port = 0;

	Xml_node libc_node = config()->xml_node().sub_node("libc");
	String<ADDR_STR_SZ> ip_addr_str =
		read_string_attribute<ADDR_STR_SZ>(libc_node, "ip_addr", String<ADDR_STR_SZ>());
	String<ADDR_STR_SZ> netmask_str =
		read_string_attribute<ADDR_STR_SZ>(libc_node, "netmask", String<ADDR_STR_SZ>());
	String<ADDR_STR_SZ> gateway_str =
		read_string_attribute<ADDR_STR_SZ>(libc_node, "gateway", String<ADDR_STR_SZ>());

	try { libc_node.attribute("http_port").value(&port); }
	catch(...) {
		error("Missing \"http_port\" attribute.");
		throw Xml_node::Nonexistent_attribute();
	}

	log("static network interface: ip=", ip_addr_str, " nm=", netmask_str, " gw=", gateway_str);

	ip = inet_addr(ip_addr_str.string());
	nm = inet_addr(netmask_str.string());
	gw = inet_addr(gateway_str.string());

	if (ip == INADDR_NONE || nm == INADDR_NONE || gw == INADDR_NONE) {
		error("Invalid network interface config.");
		throw -1;
	}

	/* Initialize network stack  */
	if (lwip_nic_init(ip, nm, gw, BUF_SIZE, BUF_SIZE)) {
		error("got no IP address!");
		return -1;
	}

	log("Create new socket ...");
	if((s = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		error("no socket available!");
		return -1;
	}

	Genode::snprintf(
		http_index_html, HTTP_INDEX_HTML_SZ,
		"<html><head></head><body>"
		"<h1>HTTP server at %s:%u</h1>"
		"<p>This is a small test page.</body></html>",
		ip_addr_str.string(), port);

	log("Now, I will bind ...");
	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons(port);
	in_addr.sin_addr.s_addr = INADDR_ANY;
	if(lwip_bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr))) {
		error("bind failed!");
		return -1;
	}

	log("Now, I will listen ...");
	if(lwip_listen(s, 5)) {
		error("listen failed!");
		return -1;
	}

	log("Start the server loop ...");
	while(true) {
		struct sockaddr addr;
		socklen_t len = sizeof(addr);
		int client = lwip_accept(s, &addr, &len);
		if(client < 0) {
			warning("invalid socket from accept!");
			continue;
		}
		http_server_serve(client);
		lwip_close(client);
	}
	return 0;
}
