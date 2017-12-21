/*
 * \brief   Timed event scheduler interface
 * \date    2005-10-24
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__ALARM_H_
#define _INCLUDE__OS__ALARM_H_

#include <base/lock.h>

namespace Genode {
	class Alarm_scheduler;
	class Alarm;
}


class Genode::Alarm
{
	public:

		typedef unsigned long Time;

	private:

		friend class Alarm_scheduler;

		struct Raw
		{
			Time deadline;       /* next deadline                */
			bool deadline_period;
			Time period;         /* duration between alarms      */

			bool is_pending_at(unsigned long time, bool time_period) const;
		};

		Lock             _dispatch_lock { };          /* taken during handle method   */
		Raw              _raw           { };
		int              _active        { 0 };        /* set to one when active       */
		Alarm           *_next          { nullptr };  /* next alarm in alarm list     */
		Alarm_scheduler *_scheduler     { nullptr };  /* currently assigned scheduler */

		void _assign(Time             period,
		             Time             deadline,
		             bool             deadline_period,
		             Alarm_scheduler *scheduler)
		{
			_raw.period          = period;
			_raw.deadline_period = deadline_period;
			_raw.deadline        = deadline;
			_scheduler           = scheduler;
		}

		void _reset() {
			_assign(0, 0, false, 0), _active = 0, _next = 0; }

		/*
		 * Noncopyable
		 */
		Alarm(Alarm const &);
		Alarm &operator = (Alarm const &);

	protected:

		/**
		 * Method to be called on when deadline is reached
		 *
		 * This method must be implemented by a derived class.  If the
		 * return value is 'true' and the alarm is periodically scheduled,
		 * the alarm is scheduled again.
		 */
		virtual bool on_alarm(unsigned) { return false; }

	public:

		Alarm() { _reset(); }

		virtual ~Alarm();
};


class Genode::Alarm_scheduler
{
	private:

		Lock         _lock       { };         /* protect alarm list                     */
		Alarm       *_head       { nullptr }; /* head of alarm list                     */
		Alarm::Time  _now        { 0UL };     /* recent time (updated by handle method) */
		bool         _now_period { false };
		Alarm::Raw   _min_handle_period { };

		/**
		 * Enqueue alarm into alarm queue
		 *
		 * This is a helper for 'schedule' and 'handle'.
		 */
		void _unsynchronized_enqueue(Alarm *alarm);

		/**
		 * Dequeue alarm from alarm queue
		 */
		void _unsynchronized_dequeue(Alarm *alarm);

		/**
		 * Dequeue next pending alarm from alarm list
		 *
		 * \return  dequeued pending alarm
		 * \retval  0  no alarm pending
		 */
		Alarm *_get_pending_alarm();

		/**
		 * Assign timeout values to alarm object and add it to the schedule
		 */
		void _setup_alarm(Alarm &alarm, Alarm::Time period, Alarm::Time deadline);

		/*
		 * Noncopyable
		 */
		Alarm_scheduler(Alarm_scheduler const &);
		Alarm_scheduler &operator = (Alarm_scheduler const &);

	public:

		Alarm_scheduler(Alarm::Time min_handle_period = 1)
		{
			Alarm::Time const deadline         = _now + min_handle_period;
			_min_handle_period.period          = min_handle_period;
			_min_handle_period.deadline        = deadline;
			_min_handle_period.deadline_period = _now > deadline ?
			                                     !_now_period : _now_period;
		}

		~Alarm_scheduler();

		/**
		 * Schedule absolute timeout
		 *
		 * \param timeout  absolute point in time for execution
		 */
		void schedule_absolute(Alarm *alarm, Alarm::Time timeout);

		/**
		 * Schedule alarm (periodic timeout)
		 *
		 * \param period  alarm period
		 *
		 * The first deadline is overdue after this call, i.e. on_alarm() is
		 * called immediately.
		 */
		void schedule(Alarm *alarm, Alarm::Time period);

		/**
		 * Remove alarm from schedule
		 */
		void discard(Alarm *alarm);

		/**
		 * Handle alarms
		 *
		 * \param now  current time
		 */
		void handle(Alarm::Time now);

		/**
		 * Determine next deadline (absolute)
		 *
		 * \param deadline  out parameter for storing the next deadline
		 * \return          true if an alarm is scheduled
		 */
		bool next_deadline(Alarm::Time *deadline);

		/**
		 * Determine if given alarm object is current head element
		 *
		 * \param alarm  alarm object
		 * \return true if alarm is head element of timeout queue
		 */
		bool head_timeout(const Alarm * alarm) { return _head == alarm; }
};

#endif /* _INCLUDE__OS__ALARM_H_ */
