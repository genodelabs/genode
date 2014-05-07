/*
 * \brief  Test for IPC send and wait via Genode's IPC framework
 * \author Norman Feske
 * \date   2009-03-26
 *
 * This program can be started as roottask replacement directly on the
 * OKL4 kernel.
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

static Untyped_capability receiver_cap;


/**
 * Sender thread, must not be started before 'receiver_cap' is initialized
 */
static void sender_thread_entry()
{
	thread_init_myself();

	static Msgbuf<256> sndbuf;
	static Ipc_ostream os(receiver_cap, &sndbuf);

	int a = 1, b = 2, c = 3;

	printf("sending a=%d, b=%d, c=%d\n", a, b, c);
	os << a << b << c << IPC_SEND;

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

	static Msgbuf<256> rcvbuf;
	static Ipc_istream is(&rcvbuf);

	/* make input stream capability known */
	receiver_cap = is;

	/* create sender thread, sending to destination (us) */
	enum { THREAD_STACK_SIZE = 4096 };
	static int thread_stack[THREAD_STACK_SIZE];
	create_thread(1, L4_rootserverno,
	              (void *)(&thread_stack[THREAD_STACK_SIZE]),
	              sender_thread_entry);

	/* wait for incoming IPC */
	int a = 0, b = 0, c = 0;
	is >> IPC_WAIT >> a >> b >> c;
	printf("received a=%d, b=%d, c=%d\n", a, b, c);

	printf("exiting main()\n");
	return 0;
}
