/*
 * \brief  HTTP client test
 * \author Ivan Loskutov
 * \date   2012-12-21
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/thread.h>
#include <util/string.h>
#include <timer_session/connection.h>
#include <nic/packet_allocator.h>

extern "C" {
#include <lwip/sockets.h>
#include <lwip/api.h>
#include <netif/etharp.h>
}

#include <lwip/genode.h>


static const char *http_get_request =
"GET / HTTP/1.0\r\nHost: localhost:80\r\n\r\n"; /* simple HTTP request header */


/**
 * The client thread simply loops endless,
 * and sends as much 'http get' requests as possible,
 * printing out the response.
 */
int main()
{
	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	static Timer::Connection _timer;
	lwip_tcpip_init();

	char serv_addr[] = "10.0.2.55";

	if( lwip_nic_init(0, 0, 0, BUF_SIZE, BUF_SIZE))
	{
		PERR("We got no IP address!");
		return 0;
	}

	for(int j = 0; j != 5; ++j) {
		_timer.msleep(2000);


		PDBG("Create new socket ...");
		int s = lwip_socket(AF_INET, SOCK_STREAM, 0 );
		if (s < 0) {
			PERR("No socket available!");
			continue;
		}

		PDBG("Connect to server ...");
		struct sockaddr_in addr;
		addr.sin_port = htons(80);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(serv_addr);

		if((lwip_connect(s, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
			PERR("Could not connect!");
			lwip_close(s);
			continue;
		}

		PDBG("Send request...");
		unsigned long bytes = lwip_send(s, (char*)http_get_request,
		                                Genode::strlen(http_get_request), 0);
		if ( bytes < 0 ) {
			PERR("Couldn't send request ...");
			lwip_close(s);
			continue;
		}

		/* Receive http header and content independently in 2 packets */
		for(int i=0; i<2; i++) {
			char buf[1024];
			ssize_t buflen;
			buflen = lwip_recv(s, buf, 1024, 0);
			if(buflen > 0) {
				buf[buflen] = 0;
				PDBG("Packet received!");
				PDBG("Packet content:\n%s", buf);
			} else
				break;
		}

		/* Close socket */
		lwip_close(s);
	}

	return 0;
}
