/*
 * \brief  Minimal HTTP server demonstration using socket API
 * \author Josef Soentgen
 * \date   2016-03-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* libc includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>


const static char http_html_hdr[] =
	"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"; /* HTTP response header */

const static char http_index_html[] =
	"<html><head><title>Congrats!</title></head><body><h1>Welcome to our HTTP demonstration server!</h1><p>This is a small test page.</body></html>"; /* HTML page */

static void serve(int fd) {
	char    buf[1024];
	ssize_t buflen;

	/* Read the data from the port, blocking if nothing yet there.
	   We assume the request (the part we care about) is in one packet */
	buflen = recv(fd, buf, 1024, 0);

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

			/* Send http header */
			send(fd, http_html_hdr, sizeof(http_html_hdr), 0);

			/* Send our HTML page */
			send(fd, http_index_html, sizeof(http_index_html), 0);
		}
	}
}


int main()
{
	int s;

	Genode::log("create new socket ...");
	if((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		Genode::error("no socket available!");
		return -1;
	}

	Genode::log("Now, I will bind ...");
	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons(80);
	in_addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr))) {
		Genode::error("bind failed!");
		return -1;
	}

	Genode::log("Now, I will listen ...");
	if(listen(s, 5)) {
		Genode::error("listen failed!");
		return -1;
	}

	Genode::log("Start the server loop ...");
	while(true) {
		struct sockaddr addr;
		socklen_t len = sizeof(addr);
		int client = accept(s, &addr, &len);
		if(client < 0) {
			Genode::warning("invalid socket from accept!");
			continue;
		}
		serve(client);
		close(client);
	}
	return 0;
}
