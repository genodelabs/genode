/*
 * \brief  Minimal HTTP server lwIP demonstration
 * \author lwIP Team
 * \author Stefan Kalkowski
 * \author Martin Stein
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

using namespace Genode;

const static char http_html_hdr[] =
	"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"; /* HTTP response header */

const static char http_index_html[] =
	"<html><head><title>Congrats!</title></head><body><h1>Welcome to our lwIP HTTP server!</h1><p>This is a small test page.</body></html>"; /* HTML page */


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
	buflen = recv(conn, buf, 1024, 0);
	puts("Packet received!");

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

			puts("Will send response");

			/* Send http header */
			send(conn, http_html_hdr, Genode::strlen(http_html_hdr), 0);

			/* Send our HTML page */
			send(conn, http_index_html, Genode::strlen(http_index_html), 0);
		}
	}
}


static void test(Libc::Env &env)
{
	Attached_rom_dataspace config(env, "config");
	uint16_t const port = config.xml().attribute_value("port", (uint16_t)80);

	puts("Create new socket ...");
	int s;
	if((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		error("no socket available!");
		env.parent().exit(-1);
	}

	puts("Now, I will bind ...");
	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons(port);
	in_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr))) {
		fprintf(stderr, "bind failed!\n");
		env.parent().exit(-1);
	}

	puts("Now, I will listen ...");
	if (listen(s, 5)) {
		fprintf(stderr, "listen failed!\n");
		env.parent().exit(-1);
	}

	puts("Start the server loop ...");
	while (true) {
		struct sockaddr addr;
		socklen_t len = sizeof(addr);
		int client = accept(s, &addr, &len);
		if(client < 0) {
			fprintf(stderr, "invalid socket from accept!\n");
			continue;
		}
		http_server_serve(client);
		close(client);
	}
	env.parent().exit(0);
}


void Libc::Component::construct(Libc::Env &env) { with_libc([&] () { test(env); }); }
