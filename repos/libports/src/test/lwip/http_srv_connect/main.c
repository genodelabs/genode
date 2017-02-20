/*
 * \brief  Simple http_srv client
 * \author Josef Soentgen
 * \date   2012-8-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

/* prototypes for net.c functions */
extern int dial(struct addrinfo*);
extern struct addrinfo *lookup(const char*, const char*, const char*);

/* pthread error handling */
#define handle_error_en(en, msg) \
	do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

/* send request, currently hardcoded and works with httpsrv test only */
const char req[] = "GET /";

/* thread arguments */
struct args {
	struct addrinfo* ai;
	int flags;
	int count;
	int verbose;
};

/* receive flag */
enum { F_NONE, F_RECV };

/* pthread start function */
void *
run(void *arg)
{
	char buf[1 << 20]; /* 1 MiB should be enough */
	struct args *ap = (struct args *) arg;
	int count = ap->count;
	int flags = ap->flags;
	int verbose = ap->verbose;
	int i, s, nbytes, sum_nbytes;

	for (i = 0; i < count; i++) {
		s = dial(ap->ai); /* create socket and connect */
		if (s == -1)
			break;

		/* only send/receive if flag is set { */
		if (flags & F_RECV) {
			/* first send request and second get header */
			write(s, req, sizeof(req));
			nbytes = read(s, buf, sizeof (buf));

			/* get data */
			sum_nbytes = 0;
#if UGLY_MEASURE_TIME
			struct timeval start, end;
			gettimeofday(&start, NULL);
#endif
			do {
				/**
				 * override buffer because we don't care about
				 * the actual data.
				 */
				nbytes = read(s, buf, sizeof (buf));
				sum_nbytes += nbytes;

			} while (nbytes > 0 && nbytes != -1);

			if (nbytes == -1)
				perror("read");

#if UGLY_MEASURE_TIME
			gettimeofday(&end, NULL);
			int sec = end.tv_sec - start.tv_sec;
			int usec = end.tv_usec - start.tv_usec;
			if (usec < 0) {
				usec += 1000000;
				sec--;
			}

			printf("time: %d ms\n",
			       ((sec) * 1000) + ((usec) / 1000));
#endif

			if (verbose)
				printf("bytes read: %d\n", sum_nbytes);
		}
		/* } */

		close(s);
	}
	/* XXX s == -1 handling */

	return NULL;
}

int
main(int argc, char *argv[])
{
	struct args a;
	struct addrinfo *ai;
	const char *proto = "tcp", *host, *port;
	int i, err, verbose = 0, flags = F_NONE, count = 1, threads = 1;
	pthread_t *thread;

	if (argc < 3) {
		fprintf(stderr, "usage: %s [-vr] [-c count] [-t threads] "
		                "[-p protocol] <host> <port>\n", argv[0]);
		exit(1);
	}

	/* argument parsing { */
	while ((i = getopt(argc, argv, "c:p:t:rv")) != -1) {
		switch (i) {
		case 'c': count = atoi(optarg);   break;
		/**
		 * valid values are only 'tcp' and 'udp'
		 */
		case 'p': proto = optarg;         break;
		case 't': threads = atoi(optarg); break;
		/**
		 * If receive is specified we will read from the socket,
		 * just connect to the host otherwise.
		 */
		case 'r': flags |= F_RECV;        break;
		case 'v': verbose = 1;            break;
		default:                          break;
		}
	}

	if ((argc - optind) != 2) {
		fprintf(stderr, "missing host and/or port\n");
		exit(1);
	}

	host = argv[optind++];
	port = argv[optind];
	/* } */

	/* main { */
	thread = (pthread_t *) calloc(threads, sizeof (pthread_t));
	if (thread == NULL) {
		perror("calloc");
		exit(-1);
	}

	/* get addrinfo once and use it for all threads */
	ai = lookup(proto, host, port);
	if (ai == NULL) {
		exit(-1);
	}

	/* fill thread argument structure */
	a.ai = ai;
	a.count = count / threads; /* XXX check if -c > -t */
	a.flags = flags;
	a.verbose = verbose;

	printf("connect to '%s!%s!%s' roughly %d times, %d per thread\n",
	       proto, host, port, count, a.count);

	for (i = 0; i < threads; i++) {
		err = pthread_create(&thread[i], NULL, run, (void*)&a);
		if (err != 0)
			handle_error_en(err, "pthread_create");
	}

	for (i = 0; i < threads; i++) {
		err = pthread_join(thread[i], NULL);
		if (err != 0)
			handle_error_en(err, "pthread_create");
	}

	freeaddrinfo(ai);
	free(thread);

	/* } */

	return 0;
}
