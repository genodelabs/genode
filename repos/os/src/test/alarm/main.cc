/*
 * \brief  Test for alarm library
 * \author Norman Feske
 * \date   2008-11-05
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <os/alarm.h>
#include <base/thread.h>
#include <base/sleep.h>
#include <base/printf.h>
#include <timer_session/connection.h>

using namespace Genode;


class Alarm_thread : Thread<4096>, public Alarm_scheduler
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
		Alarm_thread(): Thread("alarm"), _curr_time(0) { start(); }

		Alarm::Time curr_time() { return _curr_time; }
};


class One_shot_alarm : public Alarm
{
	private:

		const char *_name;

	public:

		One_shot_alarm(const char *name, Alarm_scheduler *scheduler, Time absolute_timeout):
			_name(name)
		{
			printf("scheduling one-shot alarm %s for %d msecs\n",
			       _name, (int)absolute_timeout);
			scheduler->schedule_absolute(this, absolute_timeout);
		}

	protected:

		bool on_alarm(unsigned) override
		{
			printf("one-shot alarm %s triggered\n", _name);
			return false;
		}
};


class Periodic_alarm : public Alarm
{
	private:

		const char *_name;

	public:

		Periodic_alarm(const char *name, Alarm_scheduler *scheduler, Time period):
			_name(name)
		{
			printf("scheduling periodic alarm %s for period of %d msecs\n", _name, (int)period);
			scheduler->schedule(this, period);
		}

	protected:

		bool on_alarm(unsigned) override
		{
			printf("periodic alarm %s triggered\n", _name);
			return true;
		}
};


int main(int, char **)
{
	static Alarm_thread alarm_thread;

	static Periodic_alarm pa1("Period_1s",   &alarm_thread, 1000);
	static Periodic_alarm pa2("Period_700ms",&alarm_thread, 700);
	static One_shot_alarm os1("One_shot_3s", &alarm_thread, alarm_thread.curr_time() + 3*1000);
	static One_shot_alarm os2("One_shot_5s", &alarm_thread, alarm_thread.curr_time() + 5*1000);

	sleep_forever();
	return 0;
}
