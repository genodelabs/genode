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

		Lock             _dispatch_lock;  /* taken during handle method   */
		Time             _deadline;       /* next deadline                */
		Time             _period;         /* duration between alarms      */
		int              _active;         /* set to one when active       */
		Alarm           *_next;           /* next alarm in alarm list     */
		Alarm_scheduler *_scheduler;      /* currently assigned scheduler */

		void _assign(Time period, Time deadline, Alarm_scheduler *scheduler) {
			_period = period, _deadline = deadline, _scheduler = scheduler; }

		void _reset() {
			_assign(0, 0, 0), _active = 0, _next = 0; }

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

		Lock         _lock;   /* protect alarm list                     */
		Alarm       *_head;   /* head of alarm list                     */
		Alarm::Time  _now;    /* recent time (updated by handle method) */

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

	public:

		Alarm_scheduler() : _head(0), _now(0) { }
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
