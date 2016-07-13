/*
 * \brief  Test for the 'Thread::join()' function
 * \author Norman Feske
 * \date   2012-11-16
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <base/thread.h>
#include <timer_session/connection.h>

using namespace Genode;


struct Worker : Genode::Thread_deprecated<4096>
{
	Timer::Session   &timer;
	unsigned    const result_value;
	unsigned volatile result;

	void entry()
	{
		log("worker thread is up");
		timer.msleep(250);

		log("worker is leaving the entry function with "
		    "result=", result_value, "...");
		result = result_value;
	}

	Worker(Timer::Session &timer, int result_value)
	:
		Thread_deprecated("worker"),
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
	log("--- thread join test ---");

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
			error("work remains unfinished after 'join()' returned");
			return -1;
		}
	}

	log("--- signalling test finished ---");
	return 0;
}
