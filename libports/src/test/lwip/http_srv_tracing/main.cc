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

namespace Fiasco {
#include <l4/sys/ktrace.h>
}


//enum { FILE_SIZE = 1UL };        /*  1 Byte */
enum { FILE_SIZE = 3072UL };     /*  3 KiB */
//enum { FILE_SIZE = 5120UL };     /*  5 KiB */
//enum { FILE_SIZE = 8388608UL };  /*  8 MiB */
//enum { FILE_SIZE = 2097152UL };  /*  2 MiB */
//enum { FILE_SIZE = 16777216UL }; /* 16 MiB */
//enum { FILE_SIZE = 33554432UL }; /* 32 MiB */

const static char http_html_hdr[] =
	"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"; /* HTTP response header */

static char http_file_data[FILE_SIZE];


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
	//PLOG("Packet received!");

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

			//PLOG("Will send response");

			/* Send http header */
			//Fiasco::fiasco_tbuf_log_3val(">>    lwip_send", Genode::strlen(http_html_hdr), 0, 0);
			lwip_send(conn, http_html_hdr, Genode::strlen(http_html_hdr), 0);
			//Fiasco::fiasco_tbuf_log("<<    lwip_send");

			/*
			unsigned int val = 0xdeadbeef;
			Fiasco::fiasco_tbuf_log_3val(">>    lwip_send", sizeof (unsigned int), 0, 0);
			lwip_send(conn, &val, sizeof (unsigned int), 0);
			Fiasco::fiasco_tbuf_log("<<    lwip_send");
			*/

			lwip_send(conn, http_file_data, sizeof (http_file_data), 0);
		}
	}
}

#include <l4/sys/kdebug.h>

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
	//	PLOG("Sent response, closing connection");
		lwip_close(client);
	}

	return 0;
}
