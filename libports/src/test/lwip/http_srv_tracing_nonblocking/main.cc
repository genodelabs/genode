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
#include <errno.h>

//enum { FILE_SIZE = 8388608UL }; /* 8 MiB */
//enum { FILE_SIZE = 2097152UL }; /* 2 MiB */
//enum { FILE_SIZE = 16777216UL }; /* 16 MiB */
//enum { FILE_SIZE = 33554432UL }; /* 32 MiB */
enum { FILE_SIZE = 5120UL }; /* 5 KiB */

enum { MAX_CLIENTS = 1024 };
enum { TRY_TO_CONNECT = 100 };

const static char http_html_hdr[] =
	"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"; /* HTTP response header */

static char http_file_data[FILE_SIZE];

struct clients {
	int fd;
	struct sockaddr addr;
	socklen_t len;
};

static struct clients c[MAX_CLIENTS];

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
			lwip_send(conn, http_file_data, FILE_SIZE, 0);
		}
	}
}

#include <l4/sys/kdebug.h>

int main()
{
	int s, c_num;

	fd_set rs, ws, es;

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

	PLOG("Now, I will bind to port 80 ...");
	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons(80);
	in_addr.sin_addr.s_addr = INADDR_ANY;
	memset(&(in_addr.sin_zero), '\0', 8);

	if (lwip_bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr))) {
		PERR("bind failed!");
		return -1;
	}

	PLOG("Now, I will listen ...");
	if (lwip_listen(s, 5)) {
		PERR("listen failed!");
		return -1;
	}

	PLOG("Make socket non-blocking ...");
	if (lwip_fcntl(s, F_SETFL, O_NONBLOCK)) {
		PERR("fcntl() failed!");
		return -1;
	}

	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
		c[i].fd = -1;

	c_num = 0;

	PLOG("Start the server loop ...");
	while(true) {
		/* clear fds */
		FD_ZERO(&rs);
		FD_ZERO(&ws);
		FD_ZERO(&es);

		/* set fds */
		FD_SET(s, &rs);
		for (int i = 0, num = c_num; i < MAX_CLIENTS; i++) {
			if (c[i].fd != -1) {
				FD_SET(c[i].fd, &rs);

				if (num > 0)
					num--;
				else
					break;
			}
		}

		//PLOG("before select, c_num: %d", c_num);
		int ready = lwip_select(c_num + 1, &rs, &ws, &es, &timeout);

		if (ready > 0) {
			if (FD_ISSET(s, &rs)) {
				for (int i = 0; i < TRY_TO_CONNECT; i++) {
					int *fd               = &c[c_num].fd;
					struct sockaddr *addr = &c[c_num].addr;
					socklen_t *len        = &c[c_num].len;

					*fd = lwip_accept(s, addr, len);

					if (*fd < 0) {
						/* there is currently nobody waiting */
						if (errno == EWOULDBLOCK)
							break;

						//PWRN("Invalid socket from accept!");
						continue;
					}
					if (lwip_fcntl(*fd, F_SETFL, O_NONBLOCK)) {
						//PERR("fcntl() failed");
						lwip_close(*fd);
						continue;
					}

					c_num++;
					//PLOG("add client %d", c_num);
				}
			}

			for (int i = 0, num = c_num; i < MAX_CLIENTS; i++) {
				int *fd = &c[i].fd;

				if (*fd != -1) {
					if (FD_ISSET(*fd, &rs)) {
						http_server_serve(*fd);
						//PLOG("Send response, closing connection");
						lwip_close(*fd);

						c_num--;
						*fd = -1;

						//PLOG("after close, c_num: %d", c_num);
					}

					if (num > 0)
						num--;
					else
						break;
				}
			}
		}
	}

	return 0;
}
