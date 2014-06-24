/*
 * \brief   Timed event scheduler
 * \date    2005-10-24
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <os/alarm.h>

using namespace Genode;


void Alarm_scheduler::_unsynchronized_enqueue(Alarm *alarm)
{
	/* do not enqueue twice */
	if (alarm->_active)
		return;

	alarm->_active++;

	/* if alarmlist is empty add first element */
	if (!_head) {
		alarm->_next = 0;
		_head = alarm;
		return;
	}

	/* if deadline is smaller than any other deadline, put it on the head */
	if ((int)alarm->_deadline - (int)_now < (int)_head->_deadline - (int)_now) {
		alarm->_next = _head;
		_head = alarm;
		return;
	}

	/* find list element with a higher deadline */
	Alarm *curr = _head;
	while (curr->_next &&
	       (int)curr->_next->_deadline - (int)_now < (int)alarm->_deadline - (int)_now)
		curr = curr->_next;

	/* if end of list is reached, append new element */
	if (curr->_next == 0) {
		curr->_next = alarm;
		return;
	}

	/* insert element in middle of list */
	alarm->_next = curr->_next;
	curr->_next  = alarm;
}


void Alarm_scheduler::_unsynchronized_dequeue(Alarm *alarm)
{
	if (!_head) return;

	if (_head == alarm) {
		_head = alarm->_next;
		alarm->_reset();
		return;
	}

	/* find predecessor in alarm queue */
	Alarm *curr;
	for (curr = _head; curr && (curr->_next != alarm); curr = curr->_next);

	/* alarm is not enqueued */
	if (!curr) return;

	/* remove alarm from alarm queue */
	curr->_next = alarm->_next;
	alarm->_reset();
}


Alarm *Alarm_scheduler::_get_pending_alarm()
{
	Lock::Guard lock_guard(_lock);

	if (!_head || ((int)_head->_deadline - (int)_now >= 0))
		return 0;

	/* remove alarm from head of the list */
	Alarm *pending_alarm = _head;
	_head = _head->_next;

	/*
	 * Acquire dispatch lock to defer destruction until the call of 'on_alarm'
	 * is finished
	 */
	pending_alarm->_dispatch_lock.lock();

	/* reset alarm object */
	pending_alarm->_next = 0;
	pending_alarm->_active--;

	return pending_alarm;
}


void Alarm_scheduler::handle(Alarm::Time curr_time)
{
	Alarm *curr;
	_now = curr_time;

	while ((curr = _get_pending_alarm())) {

		/* do not reschedule if alarm function returns 0 */
		bool reschedule = curr->on_alarm(1);

		if (reschedule) {

			/* schedule next event */
			if (curr->_deadline == 0)
				curr->_deadline = _now;

			curr->_deadline += curr->_period;

			/* synchronize enqueue operation */
			Lock::Guard lock_guard(_lock);
			_unsynchronized_enqueue(curr);
		}

		/* release alarm, resume concurrent destructor operation */
		curr->_dispatch_lock.unlock();
	}
}


void Alarm_scheduler::schedule_absolute(Alarm *alarm, Alarm::Time timeout)
{
	Lock::Guard alarm_list_lock_guard(_lock);

	alarm->_assign(0, timeout, this);
	_unsynchronized_enqueue(alarm);
}


void Alarm_scheduler::schedule(Alarm *alarm, Alarm::Time period)
{
	Lock::Guard alarm_list_lock_guard(_lock);

	/* first deadline is overdue */
	alarm->_assign(period, _now, this);
	_unsynchronized_enqueue(alarm);
}


void Alarm_scheduler::discard(Alarm *alarm)
{
	/*
	 * Make sure that nobody is inside the '_get_pending_alarm' when
	 * grabbing the '_dispatch_lock'. This is important when this function
	 * is called from the 'Alarm' destructor. Without the '_dispatch_lock',
	 * we could take the lock and proceed with destruction just before
	 * '_get_pending_alarm' tries to grab the lock. When the destructor is
	 * finished, '_get_pending_alarm' would proceed with operating on a
	 * dangling pointer.
	 */
	Lock::Guard alarm_list_lock_guard(_lock);

	if (alarm) {
		Lock::Guard alarm_lock_guard(alarm->_dispatch_lock);
		_unsynchronized_dequeue(alarm);
	}
}


bool Alarm_scheduler::next_deadline(Alarm::Time *deadline)
{
	Lock::Guard alarm_list_lock_guard(_lock);

	if (!_head) return false;

	if (deadline)
		*deadline = _head->_deadline;

	return true;
}


Alarm_scheduler::~Alarm_scheduler()
{
	Lock::Guard lock_guard(_lock);

	while (_head) {

		Alarm *next = _head->_next;

		/* reset alarm object */
		_head->_reset();

		/* remove from list */
		_head = next;
	}
}


Alarm::~Alarm()
{
	if (_scheduler)
		_scheduler->discard(this);
}

