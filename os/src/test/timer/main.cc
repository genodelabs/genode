/*
 * \brief  Test for timer service
 * \author Norman Feske
 * \date   2009-06-22
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/list.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <timer_session/connection.h>

enum { STACK_SIZE = 4096 };

class Timer_client : public Genode::List<Timer_client>::Element,
                     Timer::Connection, Genode::Thread<STACK_SIZE>
{
	private:

		unsigned long _period_msec;
		unsigned long _cnt;
		bool          _stop;

		/**
		 * Thread entry function
		 */
		void entry()
		{
			while (!_stop) {

				/* call timer service to block for a while */
				msleep(_period_msec);
				_cnt++;
			}
		}

	public:

		/**
		 * Constructor
		 */
		Timer_client(unsigned long period_msec)
		: _period_msec(period_msec), _cnt(0), _stop(false) { }

		/**
		 * Start calling the timer service
		 */
		void start()
		{
			Genode::Thread<STACK_SIZE>::start();
		}

		/**
		 * Stop calling the timer service
		 */
		void stop() { _stop = true; }

		/**
		 * Return configured period in milliseconds
		 */
		unsigned long period_msec() { return _period_msec; }

		/**
		 * Return the number of performed calls to the timer service
		 */
		unsigned long cnt() { return _cnt; }
};


using namespace Genode;

extern "C" int usleep(unsigned long usec);


int main(int argc, char **argv)
{
	printf("--- timer test ---\n");

	static Genode::List<Timer_client> timer_clients;
	static Timer::Connection main_timer;

	/* check long single timeout */
	printf("register two-seconds timeout...\n");
	main_timer.msleep(2000);
	printf("timeout fired\n");

	/* create timer clients with different periods */
	for (unsigned period_msec = 1; period_msec < 28; period_msec++) {
		Timer_client *tc = new (env()->heap()) Timer_client(period_msec);
		timer_clients.insert(tc);
		tc->start();
	}

	enum { SECONDS_TO_WAIT = 10 };
	for (unsigned i = 0; i < SECONDS_TO_WAIT; i++) {
		main_timer.msleep(1000);
		printf("wait %d/%d\n", i + 1, SECONDS_TO_WAIT);
	}

	/* stop all timers */
	for (Timer_client *curr = timer_clients.first(); curr; curr = curr->next())
		curr->stop();

	/* print statistics about each timer client */
	for (Timer_client *curr = timer_clients.first(); curr; curr = curr->next())
		printf("timer (period %ld ms) triggered %ld times -> slept %ld ms\n",
		       curr->period_msec(), curr->cnt(), curr->period_msec()*curr->cnt());

	printf("--- timer test finished ---\n");
	Genode::sleep_forever();

	return 0;
}
