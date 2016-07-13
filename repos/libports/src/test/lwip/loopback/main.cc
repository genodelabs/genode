/*
 * \brief  Minimal HTTP server/client lwIP loopback demonstration
 * \author lwIP Team
 * \author Stefan Kalkowski
 * \date   2009-10-23
 *
 * This example is used to show to you the usage of the loopback device
 * in LwIP, as well as how to initialize the LwIP stack by hand.
 * Note: if you simply want to use the LwIP stack together with the
 * nic-driver in Genode, you should have a look at another example,
 * e.g.: the small http-server
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <util/string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


static const char *http_get_request =
	"GET / HTTP/1.0\r\nHost: localhost:80\r\n\r\n";  /* simple HTTP request header */


/**
 * The client thread simply loops endless,
 * and sends as much 'http get' requests as possible,
 * printing out the response.
 */
class Client : public Genode::Thread_deprecated<4096>
{
	public:

		Client() : Thread_deprecated("client") { }

		void entry()
		{
			/* client loop */
			while(true) {

				Genode::log("Create new socket ...");
				int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (s < 0) {
					Genode::error("no socket available!");
					continue;
				}

				Genode::log("Connect to server ...");
				struct sockaddr_in addr;
				addr.sin_port = htons(80);
				addr.sin_family = AF_INET;
				addr.sin_addr.s_addr = inet_addr("127.0.0.1");
				if((connect(s, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
					Genode::error("could not connect!");
					close(s);
					continue;
				}

				Genode::log("Send request...");
				unsigned long bytes = send(s, (char*)http_get_request,
				                                Genode::strlen(http_get_request), 0);
				if ( bytes < 0 ) {
					Genode::error("couldn't send request ...");
					close(s);
					continue;
				}

				/* Receive http header and content independently in 2 packets */
				for(int i=0; i<2; i++) {
					char    buf[1024];
					ssize_t buflen;
					buflen = recv(s, buf, 1024, 0);
					if(buflen > 0) {
						buf[buflen] = 0;
						Genode::log("Packet received!");
						Genode::log("Packet content:\n", Genode::Cstring(buf));
					} else
						break;
				}

				/* Close socket */
				close(s);
			}
		}
};


const static char http_html_hdr[] =
	"HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n";  /* HTTP response header */
const static char http_index_html[] =
	"<html><head><title>Congrats!</title></head><body><h1>Welcome to our lwIP HTTP server!</h1><p>This is a small test page.</body></html>";  /* HTML page*/


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
	buflen = recv(conn, buf, 1024, 0);
	Genode::log("Request received!");

	/* Ignore receive errors */
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
			send(conn, http_html_hdr, Genode::strlen(http_html_hdr), 0);

			/* Send our HTML page */
			send(conn, http_index_html, Genode::strlen(http_index_html), 0);
		}
	}
}


/**
 * Server function, the server loops endless, waits for client requests
 * and responds with a html page.
 */
int server() {
	int s;

	Genode::log("Create new socket ...");
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

	/*
	 * Server loop
	 *
	 * Unblock client thread, wait for requests and handle them,
	 * after that close the connection.
	 */
	Genode::log("Start the loop ...");
	while(true) {
		struct sockaddr addr;
		socklen_t len = sizeof(addr);
		int client = accept(s, &addr, &len);
		if(client < 0) {
			Genode::warning("invalid socket from accept!");
			continue;
		}
		http_server_serve(client);
		close(client);
	}
	return 0;
}


int main()
{
	Client client;
	client.start();

	server();
	return 0;
}
