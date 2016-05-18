/*
 * \brief  Minimal datagram server demonstration using socket API
 * \author Josef Soentgen
 * \date   2016-04-22
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* libc includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>


int main(void)
{
	int s;

	PLOG("Create new socket ...");
	if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		PERR("No socket available!");
		return -1;
	}

	PLOG("Now, I will bind ...");
	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons(1337);
	in_addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(s, (struct sockaddr*)&in_addr, sizeof(in_addr))) {
		PERR("bind failed!");
		return -1;
	}

	PLOG("Start the server loop ...");
	while(true) {
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		socklen_t len = sizeof(addr);

		char buf[4096];
		memset(buf, 0, sizeof(buf));

		ssize_t n = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &len);

		if (n == 0) {
			PWRN("Invalid request!");
			continue;
		}

		if (n < 0) {
			PERR("Error %lld", n);
			break;
		}

		PLOG("Received %lld bytes", n);
		n = sendto(s, buf, n, 0, (struct sockaddr*)&addr, len);
		PLOG("Send %lld bytes back", n);
	}
	return 0;
}
