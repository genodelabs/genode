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
#include <base/printf.h>
#include <base/thread.h>
#include <util/string.h>

/* LwIP includes */
extern "C" {
#include <lwip/sockets.h>
#include <lwip/api.h>
}

#include <lwip/genode.h>


const static char http_html_hdr[] =
	"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"; /* HTTP response header */

const static char http_index_html[] =
	"<html><head><title>Congrats!</title></head><body><h1>Welcome to our lwIP HTTP server!</h1><p>This is a small test page.</body></html>"; /* HTML page */


/**
 * Handle a single client's request.
 *
 * \param conn  socket connected to the client
 */
void http_server_serve(int conn) {
	char    buf[1024];
	ssize_t buflen;

	/* Read the data from the port, blocking if nothing yet there.
	   We assume the request (the part we care about) is in one packet */
	buflen = lwip_recv(conn, buf, 1024, 0);
	PLOG("Packet received!");

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

			PLOG("Will send response");

			/* Send http header */
			lwip_send(conn, http_html_hdr, Genode::strlen(http_html_hdr), 0);

			/* Send our HTML page */
			lwip_send(conn, http_index_html, Genode::strlen(http_index_html), 0);
		}
	}
}


int main()
{
	int s;

	lwip_tcpip_init();

	/* Initialize network stack and do DHCP */
	if (lwip_nic_init(0, 0, 0)) {
		PERR("We got no IP address!");
		return -1;
	}

	PLOG("Create new socket ...");
	if((s = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		PERR("No socket available!");
		return -1;
	}

	PLOG("Now, I will bind ...");
	struct sockaddr_in in_addr;
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(80);
    in_addr.sin_addr.s_addr = INADDR_ANY;
	if(lwip_bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr))) {
		PERR("bind failed!");
		return -1;
	}

	PLOG("Now, I will listen ...");
	if(lwip_listen(s, 5)) {
		PERR("listen failed!");
		return -1;
	}

	PLOG("Start the server loop ...");
	while(true) {
		struct sockaddr addr;
		socklen_t len = sizeof(addr);
		int client = lwip_accept(s, &addr, &len);
		if(client < 0) {
			PWRN("Invalid socket from accept!");
			continue;
		}
		http_server_serve(client);
		lwip_close(client);
	}
	return 0;
}
