/*
 * \brief  Test for the 'Thread_base::join()' function
 * \author Norman Feske
 * \date   2012-11-16
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/thread.h>
#include <timer_session/connection.h>

using namespace Genode;


struct Worker : Genode::Thread<4096>
{
	Timer::Session   &timer;
	unsigned    const result_value;
	unsigned volatile result;

	void entry()
	{
		PLOG("worker thread is up");
		timer.msleep(250);

		PLOG("worker is leaving the entry function with result=%u...",
		     result_value);
		result = result_value;
	}

	Worker(Timer::Session &timer, int result_value)
	:
		Thread("worker"),
		timer(timer), result_value(result_value), result(~0)
	{
		start();
	}
};


/**
 * Main program
 */
int main(int, char **)
{
	printf("--- thread join test ---\n");

	Timer::Connection timer;

	for (unsigned i = 0; i < 10; i++) {

		/*
		 * A worker thread is created in each iteration. Just before
		 * leaving the entry function, the worker assigns the result
		 * to 'Worker::result' variable. By validating this value,
		 * we determine whether the worker has finished or not.
		 */
		Worker worker(timer, i);
		worker.join();

		if (worker.result != i) {
			PERR("work remains unfinished after 'join()' returned");
			return -1;
		}
	}

	printf("--- signalling test finished ---\n");
	return 0;
}
