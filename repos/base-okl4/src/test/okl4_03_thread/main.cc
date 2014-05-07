/*
 * \brief  Test for using OKL4 system-call bindings for thread creation
 * \author Norman Feske
 * \date   2009-03-25
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

/* local includes */
#include "../mini_env.h"
#include "../create_thread.h"


/**
 * Global variable, modified by the thread, observed by the main thread
 */
static volatile int counter;


/**
 * Thread entry function
 */
static void thread_entry(void)
{
	/* infinite busy loop incrementing a global variable */
	while (1)
		counter++;
}


/**
 * Main program
 */
int main()
{
	using namespace Genode;
	using namespace Okl4;

	/* set default priority for ourself to make round-robin scheduling work */
	L4_Set_Priority(L4_Myself(), DEFAULT_PRIORITY);

	/* stack used for the new thread */
	enum { THREAD_STACK_SIZE = 4096 };
	static int thread_stack[THREAD_STACK_SIZE];

	/* stack grows from top, hence we specify the end of the stack */
	create_thread(1, L4_rootserverno,
	              (void *)(&thread_stack[THREAD_STACK_SIZE]),
	              thread_entry);

	/* observe the work done by the new thread */
	enum { COUNT_MAX = 10*1000*1000 };
	printf("main thread: let new thread count to %d\n", (int)COUNT_MAX);

	while (counter < COUNT_MAX) {

		printf("main thread: counter=%d\n", counter);

		/*
		 * Yield the remaining time slice to the new thread to
		 * avoid printing the same counter value again and again.
		 */
		L4_Yield();
	}

	printf("exiting main()\n");
	return 0;
}
