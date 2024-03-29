/*
 * \brief  Libc tcp send and recv test
 * \author Emery Hemingway
 * \date   2018-09-17
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Libc includes */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* PCG includes */
#include <pcg_variants.h>

enum { NUM_TEST_INTS = 1<<20, BULK_ITERATIONS = 2 };

static uint32_t data[NUM_TEST_INTS];

/* fill data array with random numbers (1) or ascending integers (0) */
static int use_random = 1;
static pcg32_random_t pcg32_rng = PCG32_INITIALIZER;

static void prepare_data()
{
	for (size_t i = 0; i < NUM_TEST_INTS; ++i)
		data[i] = use_random ? pcg32_random_r(&pcg32_rng) : i;
}

static void check_data()
{
	int bad_offset = -1;
	uint32_t expected = 0, actual = 0;

	for (size_t i = 0; i < NUM_TEST_INTS; ++i) {
		expected = use_random ? pcg32_random_r(&pcg32_rng) : i;

		if (data[i] != expected) {
			bad_offset = i << 2;
			actual     = data[i];
			break;
		}
	}

	if (bad_offset != -1) {
		fprintf(stderr, "bad data at byte offset %#x expected=%#08x got=%#08x\n", bad_offset, expected, actual);
		exit(-1);
	}
}


int test_send(char const *host)
{
	usleep(1000000);

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("`socket` failed");
		return sock;
	}

	{
		struct sockaddr_in addr;
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = inet_addr(host);
		addr.sin_port        = htons(2);

		fprintf(stderr, "connect to %s\n", host);
		if (connect(sock, (struct sockaddr *)&addr, sizeof(addr))) {
			perror("`connect` failed");
			return ~0;
		}
	}

	for (int j = 0; j < BULK_ITERATIONS; ++j) {
		prepare_data();

		size_t const total = sizeof(data);
		size_t offset = 0;

		char const *buf = (char const *)data;
		while (offset < total) {
			ssize_t res = send(sock, buf+offset, total-offset, 0);
			if (res < 1) {
				perror("send failed");
				return -1;
			}
			offset += res;
		}
	}

	fprintf(stderr, "close server\n");
	shutdown(sock, SHUT_RDWR);
	usleep(10000000);

	return 0;
}

int test_recv()
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("`socket` failed");
		return sock;
	}

	{
		struct sockaddr_in addr;
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port        = htons(2);

		if (bind(sock, (struct sockaddr *)&addr, sizeof(addr))) {
			perror("`bind` failed");
			return ~0;
		}
	}

	if (listen(sock, 1))
		perror("listen broke");

	for (;;) {
		char string[INET_ADDRSTRLEN];
		struct sockaddr_in addr;
		socklen_t super_socket_safety = sizeof(addr);

		int client = accept(sock, (struct sockaddr*)&addr, &super_socket_safety);
		if (client < 0) {
			perror("invalid socket from accept!");
			return ~0;
		}

		inet_ntop(AF_INET, &(addr.sin_addr), string, super_socket_safety);
		fprintf(stderr, "connection from %s\n", string);

		for (int j = 0; j < BULK_ITERATIONS; ++j) {

			size_t const total = sizeof(data);
			size_t offset = 0;
			char *buf = (char *)data;

			memset(buf, total, 0x55);
			while (offset < total) {
				ssize_t res = recv(client, buf+offset, total-offset, 0);
				if (res < 1) {
					perror("recv failed");
					return ~0;
				}
				offset += res;
			}

			check_data();
		}

		fprintf(stderr, "close client\n");
		shutdown(client, SHUT_RDWR);
		return 0;
	}
}

int main(int argc, char **argv)
{
	if (argc < 1) {
		fprintf(stderr, "no test name passed thru argv\n");
		return ~0;
	}
	if (argc < 2) {
		if (strcmp(argv[0], "recv") == 0)
			return test_recv();
	}
	if (argc < 3) {
		if (strcmp(argv[0], "send") == 0)
			return test_send(argv[1]);
	}

	fprintf(stderr, "\"%s\" not a valid test\n", argv[0]);
	return ~0;
}
