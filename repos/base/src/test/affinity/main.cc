/*
 * \brief  Test for setting the CPU affinity of a thread
 * \author Norman Feske
 * \date   2013-03-21
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <base/component.h>
#include <base/heap.h>


namespace Genode {

	static inline void print(Output &out, Affinity::Location location)
	{
		print(out, location.xpos(), ",", location.ypos());
	}
}


enum { STACK_SIZE = sizeof(long)*1024, COUNT_VALUE = 10 * 1024 * 1024 };

struct Spinning_thread : Genode::Thread
{
	Genode::Affinity::Location const _location;

	Genode::uint64_t volatile cnt;

	Genode::Lock barrier;

	void entry()
	{
		barrier.unlock();

		Genode::log("thread started on CPU ", _location, " spinning...");

		unsigned round = 0;

		for (;;) {
			cnt++;

			/* show a life sign every now and then... */
			if (cnt % COUNT_VALUE == 0) {
				Genode::log("thread on CPU ", _location, " keeps counting - "
				            "round ", round++, "...");
			}
		}
	}

	Spinning_thread(Genode::Env &env, Location location)
	:
		Genode::Thread(env, Name("spinning_thread"), STACK_SIZE, location,
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

	log("--- test-affinity started ---");

	Affinity::Space cpus = env.cpu().affinity_space();
	log("Detected ", cpus.width(), "x", cpus.height(), " "
	    "CPU", cpus.total() > 1 ? "s." : ".");

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

	log("Threads started on a different CPU each.");
	log("You may inspect them using the kernel debugger - if you have one.");
	log("Main thread monitors client threads and prints the status of them.");
	log("Legend : D - DEAD, A - ALIVE");

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
			log(Cstring(output_buffer));

			output = output_buffer;
			snprintf(output, sizeof(text_round), text_round, round);
			output += sizeof(text_round) - 2;

			for (unsigned i = 0; i < cpus.total(); i++) {
				snprintf(output, 4, "%s ",
				         thread_cnt[i] == threads[i]->cnt ? " D" : " A");
				output += 3;
				thread_cnt[i] = threads[i]->cnt;
			}
			log(Cstring(output_buffer));

			round ++;
		}
	}
}

void Component::construct(Genode::Env &env) { static Main inst(env); }
