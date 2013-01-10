/*
 * \brief  Test for IPC call via Genode's IPC framework
 * \author Norman Feske
 * \date   2009-03-26
 *
 * This program can be started as roottask replacement directly on the
 * OKL4 kernel. The main program plays the role of a server. It starts
 * a thread that acts as a client and performs an IPC call to the server.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc.h>

/* local includes */
#include "../mini_env.h"
#include "../create_thread.h"

using namespace Genode;
using namespace Okl4;

static Untyped_capability server_cap;


/**
 * Client thread, must not be started before 'destination' is initialized
 */
static void client_thread_entry()
{
	thread_init_myself();

	Msgbuf<256> client_rcvbuf, client_sndbuf;
	Ipc_client client(server_cap, &client_sndbuf, &client_rcvbuf);

	printf("client sends call(11, 12, 13)\n");
	int res, d = 0, e = 0;
	res = (client << 11 << 12 << 13 << IPC_CALL >> d >> e).result();
	printf("client received reply d=%d, e=%d, res=%d\n", d, e, res);

	printf("client sends call(14, 15, 16)\n");
	res = (client << 14 << 15 << 16 << IPC_CALL >> d >> e).result();
	printf("client received reply d=%d, e=%d, res=%d\n", d, e, res);

	for (;;) L4_Yield();
}


/**
 * Main program
 */
int main()
{
	roottask_init_myself();

	/* set default priority for ourself to make round-robin scheduling work */
	L4_Set_Priority(L4_Myself(), DEFAULT_PRIORITY);

	Msgbuf<256> server_rcvbuf, server_sndbuf;
	Ipc_server server(&server_sndbuf, &server_rcvbuf);

	/* make server capability known */
	server_cap = server;

	/* create client thread, making a call to the server (us) */
	enum { THREAD_STACK_SIZE = 4096 };
	static int thread_stack[THREAD_STACK_SIZE];
	create_thread(1, L4_rootserverno,
	              (void *)(&thread_stack[THREAD_STACK_SIZE]),
	              client_thread_entry);

	/* infinite server loop */
	int a = 0, b = 0, c = 0;
	for (;;) {
		printf("server: reply_wait\n");

		server >> IPC_REPLY_WAIT >> a >> b >> c;
		printf("server: received a=%d, b=%d, c=%d, send reply %d, %d, res=33\n",
		       a, b, c, a + b + c, a*b*c);

		server << a + b + c << a*b*c;
		server.ret(33);
	}
	return 0;
}
