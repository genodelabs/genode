/*
 * \brief  Timeout handling for netperf, based on test/alarm
 * \author Alexander Boettcher
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <os/alarm.h>
#include <base/thread.h>
#include <timer_session/connection.h>


using namespace Genode;

class Alarm_thread : Thread_deprecated<2048*sizeof(long)>, public Alarm_scheduler
{
	private:

		Timer::Connection _timer;
		Alarm::Time       _curr_time;   /* jiffies value */

		enum { TIMER_GRANULARITY_MSEC = 10 };

		/**
		 * Thread entry function
		 */
		void entry()
		{
			while (true) {
				_timer.msleep(TIMER_GRANULARITY_MSEC);
				Alarm_scheduler::handle(_curr_time);
				_curr_time += TIMER_GRANULARITY_MSEC;
			}
		}

	public:

		/**
		 * Constructor
		 */
		Alarm_thread(): Thread_deprecated("netperf_alarm"), _curr_time(0) { start(); }

		Alarm::Time curr_time() { return _curr_time; }
};


/* defined in "ports/contrib/netperf/src/netlib.c" */
extern "C" int times_up;


class One_shot : public Alarm
{
	private:

		Alarm_scheduler *_scheduler;

	public:

		One_shot(Alarm_scheduler *scheduler) : _scheduler(scheduler) { }

		void set_timeout(Time absolute_timeout)
		{
			_scheduler->schedule_absolute(this, absolute_timeout);
		}

	protected:

		bool on_alarm(unsigned) override
		{
			times_up = 1;
			return false;
		}
};


extern "C" void
start_timer(int time)
{
	static Alarm_thread alarm_thread;
	static One_shot oneshot(&alarm_thread);

	oneshot.set_timeout(alarm_thread.curr_time() + time * 1000);
}
