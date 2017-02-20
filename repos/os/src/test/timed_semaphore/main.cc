/*
 * \brief  Test for the timed-semaphore
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2010-03-05
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <os/timed_semaphore.h>
#include <base/thread.h>
#include <base/component.h>

using namespace Genode;


struct Test : Thread
{
	struct Failed : Exception { };

	unsigned          id;
	Timer::Connection wakeup_timer;
	unsigned const    wakeup_period;
	Timed_semaphore   sem;
	bool              stop_wakeup    { false };
	Lock              wakeup_stopped { Lock::LOCKED };
	bool              got_timeouts   { false };

	void entry()
	{
		do {
			wakeup_timer.msleep(wakeup_period);
			sem.up();
		} while (!stop_wakeup);
		wakeup_stopped.unlock();
	}

	Test(Env &env, bool timeouts, unsigned id, char const *brief)
	: Thread(env, "wakeup", 1024 * sizeof(addr_t)), id(id), wakeup_timer(env),
	  wakeup_period(timeouts ? 1000 : 100)
	{
		log("\nTEST ", id, ": ", brief, "\n");
		Thread::start();
		try { for (int i = 0; i < 10; i++) { sem.down(timeouts ? 100 : 1000); } }
		catch (Timeout_exception) { got_timeouts = true; }
		if (timeouts != got_timeouts) {
			throw Failed(); }

		stop_wakeup = true;
		wakeup_stopped.lock();
	}

	~Test() { log("\nTEST ", id, " finished\n"); }
};


struct Main
{
	Constructible<Test> test;

	Main(Env &env)
	{
		log("--- Timed semaphore test ---");
		test.construct(env, false, 1, "without timeouts"); test.destruct();
		test.construct(env, true,  2, "with timeouts");    test.destruct();
		log("--- Timed semaphore test finished ---");
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
