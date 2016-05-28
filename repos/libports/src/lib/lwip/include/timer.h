/*
 * \brief  Timer thread, which schedules timeouts.
 * \author Stefan Kalkowski
 * \date   2009-10-28
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef __LWIP__INCLUDE__TIMER_H__
#define __LWIP__INCLUDE__TIMER_H__

#include <os/alarm.h>
#include <base/thread.h>
#include <base/sleep.h>
#include <base/printf.h>
#include <timer_session/connection.h>

namespace Lwip {

	class Scheduler : public Genode::Thread_deprecated<4096>,
	                  public Genode::Alarm_scheduler
	{
		private:

			Timer::Connection   _timer;
			Genode::Alarm::Time _curr_time;   /* jiffies value */

			enum { TIMER_GRANULARITY_MSEC = 1 };

			void entry()
			{
				while (true) {
					_timer.msleep(TIMER_GRANULARITY_MSEC);
					Genode::Alarm_scheduler::handle(_curr_time);
					_curr_time += TIMER_GRANULARITY_MSEC;
				}
			}

		public:

			Scheduler() : Thread_deprecated("lwip_timeout_sched"), _curr_time(0) { }

			Genode::Alarm::Time curr_time() { return _curr_time; }
	};


	extern Scheduler *scheduler();
}

#endif //__LWIP__INCLUDE__TIMER_H__
