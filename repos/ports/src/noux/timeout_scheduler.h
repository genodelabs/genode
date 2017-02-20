/*
 * \brief  Timeout mechanism for 'select'
 * \author Norman Feske
 * \date   2011-02-14
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__TIMEOUT_SCHEDULER_H_
#define _NOUX__TIMEOUT_SCHEDULER_H_

#include <timer_session/connection.h>
#include <os/alarm.h>

namespace Noux {
	class Timeout_scheduler;
	class Timeout_state;
	class Timeout_alarm;
	using namespace Genode;
}


class Noux::Timeout_scheduler : public Alarm_scheduler
{
	private:

		Timer::Connection _timer;
		Alarm::Time       _curr_time { 0 };

		enum { TIMER_GRANULARITY_MSEC = 10 };

		Signal_handler<Timeout_scheduler> _timer_handler;

		void _handle_timer()
		{
			_curr_time = _timer.elapsed_ms();
			Alarm_scheduler::handle(_curr_time);
		}

	public:

		Timeout_scheduler(Env &env)
		:
			_timer(env),
			_timer_handler(env.ep(), *this, &Timeout_scheduler::_handle_timer)
		{
			_timer.sigh(_timer_handler);
			_timer.trigger_periodic(TIMER_GRANULARITY_MSEC*1000);
		}

		Alarm::Time curr_time() const { return _curr_time; }
};


struct Noux::Timeout_state
{
	bool timed_out;

	Timeout_state() : timed_out(false)  { }
};


class Noux::Timeout_alarm : public Alarm
{
	private:

		Timeout_state     &_state;
		Lock              &_blocker;
		Timeout_scheduler &_scheduler;

	public:

		Timeout_alarm(Timeout_state &st, Lock &blocker,
		              Timeout_scheduler &scheduler, Time timeout)
		:
			_state(st),
			_blocker(blocker),
			_scheduler(scheduler)
		{
			_scheduler.schedule_absolute(this, _scheduler.curr_time() + timeout);
			_state.timed_out = false;
		}

		void discard() { _scheduler.discard(this); }

	protected:

		bool on_alarm(unsigned) override
		{
			_state.timed_out = true;
			_blocker.unlock();

			return false;
		}
};

#endif /* _NOUX__TIMEOUT_SCHEDULER_H_ */
