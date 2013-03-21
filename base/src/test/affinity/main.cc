/*
 * \brief  Test for setting the CPU affinity of a thread
 * \author Norman Feske
 * \date   2013-03-21
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/thread.h>
#include <base/env.h>
#include <base/sleep.h>


enum { STACK_SIZE = sizeof(long)*1024 };
struct Spinning_thread : Genode::Thread<STACK_SIZE>
{
	int const cpu_number;

	unsigned long volatile cnt;

	Genode::Lock barrier;

	void entry()
	{
		PINF("thread started on CPU %d, spinning...", cpu_number);

		barrier.unlock();

		for (;;) {
			cnt++;

			/* show a life sign every now and then... */
			if (cnt > 100*1024*1024) {
				PINF("thread on CPU %d keeps counting...\n", cpu_number);
				cnt = 0;
			}
		}
	}

	Spinning_thread(unsigned cpu_number, char const *name)
	:
		Genode::Thread<STACK_SIZE>(name), cpu_number(cpu_number), cnt(0),
		barrier(Genode::Lock::LOCKED)
	{
		Genode::env()->cpu_session()->affinity(Thread_base::cap(), cpu_number);
		start();
	}
};


int main(int argc, char **argv)
{
	using namespace Genode;

	printf("--- test-affinity started ---\n");
	static Spinning_thread thread_0(0, "thread_0");
	static Spinning_thread thread_1(1, "thread_1");

	/* wait until both threads are up and running */
	thread_0.barrier.lock();
	thread_1.barrier.lock();

	printf("Threads started on a different CPU each.\n");
	printf("You may inspect them using the kernel debugger\n");

	sleep_forever();
}
