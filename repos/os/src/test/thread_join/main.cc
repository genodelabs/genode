/*
 * \brief  Test for the 'Thread::join()' function
 * \author Norman Feske
 * \date   2012-11-16
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/thread.h>
#include <timer_session/connection.h>

using namespace Genode;


struct Worker : Thread
{
	Timer::Session    &timer;
	unsigned const     result_value;
	unsigned volatile  result;

	void entry()
	{
		log("Worker thread is up");
		timer.msleep(250);

		log("Worker is leaving the entry function with result=", result_value);
		result = result_value;
	}

	Worker(Env &env, Timer::Session &timer, unsigned result_value)
	: Thread(env, "worker", 1024 * sizeof(addr_t)), timer(timer),
	  result_value(result_value), result(~0)
	{
		Thread::start();
	}
};


struct Main
{
	struct Worker_unfinished_after_join : Exception { };

	Timer::Connection timer;

	Main(Env &env) : timer(env)
	{
		log("--- Thread join test ---");
		for (unsigned i = 0; i < 10; i++) {

			/*
			 * A worker thread is created in each iteration. Just before
			 * leaving the entry function, the worker assigns the result
			 * to 'Worker::result' variable. By validating this value,
			 * we determine whether the worker has finished or not.
			 */
			Worker worker(env, timer, i);
			worker.join();
			if (worker.result != i) {
				throw Worker_unfinished_after_join(); }
		}
		log("--- Thread join test finished ---");
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
