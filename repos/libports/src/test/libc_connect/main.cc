/*
 * \brief  libc 'connect()' test
 * \author Christian Prochaska
 * \date   2019-01-11
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>


static char const *server_connected           = "10.0.1.2";
static char const *server_connection_refused  = "10.0.1.2";

/*
 * the unreachable server address must be in another nic_router 'domain'
 * so domain local IP stacks do not have direct ARP access to the address
 */
static char const *server_timeout             = "10.0.2.2";

static int const port_connected          = 80;
static int const port_connection_refused = 81;
static int const port_timeout            = 80;

#define DIE() \
	{ printf("Error: '%s' failed - %s:%d\n", __func__, __FILE__, __LINE__); exit(-1); }

static void test_blocking_connect_connected()
{
	printf("Testing blocking connect (connected)\n");

	/*
	 * This is the first test and it can happen that the server is not ready
	 * yet, so this test retries until success.
	 */
	for (;;) {
		int s = socket(AF_INET, SOCK_STREAM, 0);

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port_connected);
		addr.sin_addr.s_addr = inet_addr(server_connected);

		sockaddr const *paddr = reinterpret_cast<sockaddr const *>(&addr);

		int res = connect(s, paddr, sizeof(addr));

		if (res == 0) {

			/* keep the netty server alive */

			char send_buf = 'x';
			char receive_buf = 0;
			write(s, &send_buf, sizeof(send_buf));
			read(s, &receive_buf, sizeof(receive_buf));
			if (receive_buf != send_buf) DIE();

			close(s);
			break;
		}

		if (errno != ECONNREFUSED) DIE();

		close(s);

		printf("Warning: got 'connection refused'. "
		       "Server might not be ready yet, retrying...\n");
	}
}


static void test_blocking_connect_connection_refused()
{
	printf("Testing blocking connect (connection refused)\n");

	int s = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_connection_refused);
	addr.sin_addr.s_addr = inet_addr(server_connection_refused);

	sockaddr const *paddr = reinterpret_cast<sockaddr const *>(&addr);

	int res = connect(s, paddr, sizeof(addr));

	if (!((res == -1) && (errno == ECONNREFUSED))) DIE();

	close(s);
}


static void test_blocking_connect_timeout()
{
	printf("Testing blocking connect (timeout)\n");

	int s = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_timeout);
	addr.sin_addr.s_addr = inet_addr(server_timeout);

	sockaddr const *paddr = reinterpret_cast<sockaddr const *>(&addr);

	int res = connect(s, paddr, sizeof(addr));

	if (!((res == -1) && (errno == ETIMEDOUT))) DIE();

	close(s);
}


static void test_nonblocking_connect_connected()
{
	printf("Testing nonblocking connect (connected)\n");

	int s = socket(AF_INET, SOCK_STREAM, 0);

	fcntl(s, F_SETFL, O_NONBLOCK);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_connected);
	addr.sin_addr.s_addr = inet_addr(server_connected);

	sockaddr const *paddr = reinterpret_cast<sockaddr const *>(&addr);

	int res = connect(s, paddr, sizeof(addr));

	if (!((res == -1) && (errno == EINPROGRESS))) DIE();

	/* wait until socket is ready for writing */

	fd_set writefds;
	FD_ZERO(&writefds);
	FD_SET(s, &writefds);

	struct timeval timeout {10, 0};
	res = select(s + 1, NULL, &writefds, NULL, &timeout);

	if (res != 1) DIE();

	int so_error = 0;
	socklen_t opt_len = sizeof(so_error);

	res = getsockopt(s, SOL_SOCKET, SO_ERROR, &so_error, &opt_len);

	if (!((res == 0) && (so_error == 0))) DIE();

	res = getsockopt(s, SOL_SOCKET, SO_ERROR, &so_error, &opt_len);

	if (!((res == 0) && (so_error == 0))) DIE();

	res = connect(s, paddr, sizeof(addr));

	if (!((res == 0) || ((res == -1) && (errno == EISCONN)))) DIE();

	res = connect(s, paddr, sizeof(addr));

	if (!((res == -1) && (errno == EISCONN))) DIE();

	/* keep the netty server alive */
	
	char send_buf = 'x';
	char receive_buf = 0;

	write(s, &send_buf, sizeof(send_buf));

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(s, &readfds);

	res = select(s + 1, &readfds, NULL, NULL, &timeout);
	printf("select returned %d\n", res);

	if (res != 1) DIE();

	read(s, &receive_buf, sizeof(receive_buf));

	if (receive_buf != send_buf) DIE();

	close(s);
}


static void test_nonblocking_connect_connection_refused()
{
	printf("Testing nonblocking connect (connection refused)\n");

	int s = socket(AF_INET, SOCK_STREAM, 0);

	fcntl(s, F_SETFL, O_NONBLOCK);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_connection_refused);
	addr.sin_addr.s_addr = inet_addr(server_connection_refused);

	sockaddr const *paddr = reinterpret_cast<sockaddr const *>(&addr);

	int res = connect(s, paddr, sizeof(addr));

	if (!((res == -1) && (errno == EINPROGRESS))) DIE();

	/* wait until socket is ready for writing */

	fd_set writefds;
	FD_ZERO(&writefds);
	FD_SET(s, &writefds);

	struct timeval timeout {10, 0};
	res = select(s + 1, NULL, &writefds, NULL, &timeout);

	if (res != 1) DIE();

	int so_error = 0;
	socklen_t opt_len = sizeof(so_error);

	res = getsockopt(s, SOL_SOCKET, SO_ERROR, &so_error, &opt_len);

	if (!((res == 0) && (so_error == ECONNREFUSED))) DIE();

	res = getsockopt(s, SOL_SOCKET, SO_ERROR, &so_error, &opt_len);

	if (!((res == 0) && (so_error == 0))) DIE();

	res = connect(s, paddr, sizeof(addr));

	if (!((res == -1) && (errno == ECONNABORTED))) DIE();

	close(s);
}


static void test_nonblocking_connect_timeout()
{
	printf("Testing nonblocking connect (timeout)\n");

	int s = socket(AF_INET, SOCK_STREAM, 0);

	fcntl(s, F_SETFL, O_NONBLOCK);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_timeout);
	addr.sin_addr.s_addr = inet_addr(server_timeout);

	sockaddr const *paddr = reinterpret_cast<sockaddr const *>(&addr);

	int res = connect(s, paddr, sizeof(addr));

	if (!((res == -1) && (errno == EINPROGRESS))) DIE();

	/* wait until socket is ready for writing */

	fd_set writefds;
	FD_ZERO(&writefds);
	FD_SET(s, &writefds);

	struct timeval timeout {10, 0};
	res = select(s + 1, NULL, &writefds, NULL, &timeout);

	if (res != 0) DIE();

	close(s);
}


int main(int argc, char *argv[])
{
	test_blocking_connect_connected();
	test_blocking_connect_connection_refused();
	test_blocking_connect_timeout();

	test_nonblocking_connect_connected();
	test_nonblocking_connect_connection_refused();
	test_nonblocking_connect_timeout();

	return 0;
}
