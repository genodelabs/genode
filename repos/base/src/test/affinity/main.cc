/*
 * \brief  Test for setting the CPU affinity of a thread
 * \author Norman Feske
 * \date   2013-03-21
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/thread.h>
#include <base/component.h>
#include <base/heap.h>


enum { STACK_SIZE = sizeof(long)*1024, COUNT_VALUE = 10 * 1024 * 1024 };

struct Spinning_thread : Genode::Thread
{
	Genode::Affinity::Location const _location;

	Genode::uint64_t volatile cnt;

	Genode::Lock barrier;

	void entry()
	{
		barrier.unlock();

		PINF("thread started on CPU %d,%d, spinning...",
		     _location.xpos(), _location.ypos());

		unsigned round = 0;

		for (;;) {
			cnt++;

			/* show a life sign every now and then... */
			if (cnt % COUNT_VALUE == 0) {
				PINF("thread on CPU %d,%d keeps counting - round %u...\n",
				     _location.xpos(), _location.ypos(), round++);
			}
		}
	}

	Spinning_thread(Genode::Env &env, Location location)
	:
		Genode::Thread(env, Name("spinning_thread"), STACK_SIZE, Location(),
		               Weight(), env.cpu()),
		_location(location), cnt(0ULL), barrier(Genode::Lock::LOCKED)
	{
		start();
	}
};


struct Main
{
	Genode::Env &env;

	Genode::Heap heap { env.ram(), env.rm() };

	Main(Genode::Env &env);
};


Main::Main(Genode::Env &env) : env(env)
{
	using namespace Genode;

	printf("--- test-affinity started ---\n");

	Affinity::Space cpus = env.cpu().affinity_space();
	printf("Detected %ux%u CPU%s\n",
	       cpus.width(), cpus.height(), cpus.total() > 1 ? "s." : ".");

	/* get some memory for the thread objects */
	Spinning_thread ** threads = new (heap) Spinning_thread*[cpus.total()];
	uint64_t * thread_cnt = new (heap) uint64_t[cpus.total()];

	/* construct the thread objects */
	for (unsigned i = 0; i < cpus.total(); i++)
		threads[i] = new (heap)
		             Spinning_thread(env, cpus.location_of_index(i));

	/* wait until all threads are up and running */
	for (unsigned i = 0; i < cpus.total(); i++)
		threads[i]->barrier.lock();

	printf("Threads started on a different CPU each.\n");
	printf("You may inspect them using the kernel debugger - if you have one.\n");
	printf("Main thread monitors client threads and prints the status of them.\n");
	printf("Legend : D - DEAD, A - ALIVE\n");

	volatile uint64_t cnt = 0;
	unsigned round = 0;

	char const   text_cpu[] = "     CPU: ";
	char const text_round[] = "Round %2u: ";
	char * output_buffer = new (heap) char [sizeof(text_cpu) + 3 * cpus.total()];

	for (;;) {
		cnt++;

		/* try to get a life sign by the main thread from the remote threads */
		if (cnt % COUNT_VALUE == 0) {
			char * output = output_buffer;
			snprintf(output, sizeof(text_cpu), text_cpu);
			output += sizeof(text_cpu) - 1;
			for (unsigned i = 0; i < cpus.total(); i++) {
				snprintf(output, 4, "%2u ", i);
				output += 3;
			}
			printf("%s\n", output_buffer);

			output = output_buffer;
			snprintf(output, sizeof(text_round), text_round, round);
			output += sizeof(text_round) - 2;

			for (unsigned i = 0; i < cpus.total(); i++) {
				snprintf(output, 4, "%s ",
				         thread_cnt[i] == threads[i]->cnt ? " D" : " A");
				output += 3;
				thread_cnt[i] = threads[i]->cnt;
			}
			printf("%s\n", output_buffer);

			round ++;
		}
	}
}

Genode::size_t Component::stack_size()      { return 8*1024*sizeof(long); }
void Component::construct(Genode::Env &env) { static Main inst(env); }
