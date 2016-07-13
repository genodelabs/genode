/*
 * \brief  Test for the timed-semaphore
 * \author Stefan Kalkowski
 * \date   2010-03-05
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <timer_session/connection.h>
#include <os/timed_semaphore.h>
#include <base/thread.h>

using namespace Genode;

class Wakeup_thread : public Thread_deprecated<4096>
{
	private:

		Timed_semaphore *_sem;
		Timer::Session  *_timer;
		int              _timeout;
		Lock             _lock;
		bool             _stop;

	public:

		Wakeup_thread(Timed_semaphore *sem,
		              Timer::Session  *timer,
		              Alarm::Time      timeout)
		: Thread_deprecated("wakeup"), _sem(sem), _timer(timer), _timeout(timeout),
		  _lock(Lock::LOCKED), _stop(false) { }

		void entry()
		{
			while(true) {
				_timer->msleep(_timeout);
				_sem->up();

				if (_stop) {
					_lock.unlock();
					return;
				}
			}
		}

		void stop() { _stop = true; _lock.lock(); }

};


bool test_loop(Timer::Session *timer, Alarm::Time timeout1, Alarm::Time timeout2, int loops)
{
	Timed_semaphore sem;
	Wakeup_thread thread(&sem, timer, timeout2);
	bool ret = true;

	thread.start();
	try{
		for (int i = 0; i < loops; i++)
			sem.down(timeout1);
	} catch (Timeout_exception) {
		ret = false;
	}

	/*
	 * Explicitly stop the thread, so the destructor does not get called in
	 * unfavourable situations, e.g. where the semaphore-meta lock is still
	 * held and the semaphore destructor stalls afterwards
	 */
	thread.stop();

	return ret;
}


int main(int, char **)
{
	log("--- timed-semaphore test ---");

	Timer::Connection timer;

	log("--- test 1: good case, no timeout triggers  --");
	if(!test_loop(&timer, 1000, 100, 10)) {
		error("Test 1 failed!");
		return -1;
	}
	log("--- everything went ok  --");

	log("--- test 2: triggers timeouts --");
	if(test_loop(&timer, 100, 1000, 10)) {
		error("Test 2 failed!");
		return -2;
	}
	log("--- everything went ok  --");

	log("--- end of timed-semaphore test ---");
	return 0;
}
