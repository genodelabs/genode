/*
 * \brief  Multiplexing one time source amongst different timeout subjects
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer/timeout.h>

using namespace Genode;


/*************
 ** Timeout **
 *************/

void Timeout::schedule_periodic(Microseconds duration, Handler &handler)
{
	_alarm.handler = &handler;
	_alarm.periodic = true;
	_alarm.timeout_scheduler._schedule_periodic(*this, duration);
}


void Timeout::schedule_one_shot(Microseconds duration, Handler &handler)
{
	_alarm.handler = &handler;
	_alarm.periodic = false;
	_alarm.timeout_scheduler._schedule_one_shot(*this, duration);
}


void Timeout::discard()
{
	_alarm.timeout_scheduler._discard(*this);
	_alarm.handler = nullptr;
}


/********************
 ** Timeout::Alarm **
 ********************/

bool Timeout::Alarm::_on_alarm(uint64_t)
{
	if (handler) {
		Handler *current = handler;
		if (!periodic) {
			handler = nullptr;
		}
		current->handle_timeout(timeout_scheduler.curr_time());
	}
	return periodic;
}


Timeout::Alarm::~Alarm()
{
	if (_scheduler)
		_scheduler->_alarm_discard(this);
}


bool Timeout::Alarm::Raw::is_pending_at(uint64_t time, bool time_period) const
{
	return (time_period == deadline_period &&
	        time        >= deadline) ||
	       (time_period != deadline_period &&
	        time        <  deadline);
}


/*****************************
 ** Alarm_timeout_scheduler **
 *****************************/

void Alarm_timeout_scheduler::handle_timeout(Duration duration)
{
	uint64_t const curr_time_us = duration.trunc_to_plain_us().value;

	_alarm_handle(curr_time_us);

	/* sleep time is either until the next deadline or the maximum timout */
	uint64_t sleep_time_us;
	Alarm::Time deadline_us;
	if (_alarm_next_deadline(&deadline_us)) {
		sleep_time_us = deadline_us - curr_time_us;
	} else {
		sleep_time_us = _time_source.max_timeout().value; }

	/* limit max timeout to a more reasonable value, e.g. 60s */
	if (sleep_time_us > 60000000) {
		sleep_time_us = 60000000;
	} else if (sleep_time_us == 0) {
		sleep_time_us = 1; }

	_time_source.schedule_timeout(Microseconds(sleep_time_us), *this);
}


Alarm_timeout_scheduler::Alarm_timeout_scheduler(Time_source  &time_source,
                                                 Microseconds  min_handle_period)
:
	_time_source(time_source)
{
	Alarm::Time const deadline         = _now + min_handle_period.value;
	_min_handle_period.period          = min_handle_period.value;
	_min_handle_period.deadline        = deadline;
	_min_handle_period.deadline_period = _now > deadline ?
	                                     !_now_period : _now_period;
}


Alarm_timeout_scheduler::~Alarm_timeout_scheduler()
{
	Mutex::Guard mutex_guard(_mutex);
	while (_active_head) {
		Alarm *next = _active_head->_next;
		_active_head->_alarm_reset();
		_active_head = next;
	}
}


void Alarm_timeout_scheduler::_enable()
{
	_time_source.schedule_timeout(Microseconds(0), *this);
}


void Alarm_timeout_scheduler::_schedule_one_shot(Timeout      &timeout,
                                                 Microseconds  duration)
{
	/* raise timeout duration by the age of the local time value */
	uint64_t us = _time_source.curr_time().trunc_to_plain_us().value;
	if (us >= _now) {
		us = duration.value + (us - _now); }
	else {
		us = duration.value + (~0UL - _now) + us; }
	if (us >= duration.value) {
		duration.value = us; }

	/* insert timeout into scheduling queue */
	_alarm_schedule_absolute(&timeout._alarm, duration.value);

	/* if new timeout is the closest to now, update the time-source timeout */
	if (_alarm_head_timeout(&timeout._alarm)) {
		_time_source.schedule_timeout(Microseconds(0), *this); }
}


void Alarm_timeout_scheduler::_schedule_periodic(Timeout      &timeout,
                                                 Microseconds  duration)
{
	_alarm_schedule(&timeout._alarm, duration.value);

	if (_alarm_head_timeout(&timeout._alarm)) {
		_time_source.schedule_timeout(Microseconds(0), *this); }
}


void Alarm_timeout_scheduler::_alarm_unsynchronized_enqueue(Alarm *alarm)
{
	if (alarm->_active) {
		error("trying to insert the same alarm twice!");
		return;
	}

	alarm->_active++;

	/* if active alarm list is empty add first element */
	if (!_active_head) {
		alarm->_next = 0;
		_active_head = alarm;
		return;
	}

	/* if deadline is smaller than any other deadline, put it on the head */
	if (alarm->_raw.is_pending_at(_active_head->_raw.deadline, _active_head->_raw.deadline_period)) {
		alarm->_next = _active_head;
		_active_head = alarm;
		return;
	}

	/* find list element with a higher deadline */
	Alarm *curr = _active_head;
	while (curr->_next &&
	       curr->_next->_raw.is_pending_at(alarm->_raw.deadline, alarm->_raw.deadline_period))
	{
		curr = curr->_next;
	}

	/* if end of list is reached, append new element */
	if (curr->_next == 0) {
		curr->_next = alarm;
		return;
	}

	/* insert element in middle of list */
	alarm->_next = curr->_next;
	curr->_next  = alarm;
}


void Alarm_timeout_scheduler::_alarm_unsynchronized_dequeue(Alarm *alarm)
{
	if (!_active_head) return;

	if (_active_head == alarm) {
		_active_head = alarm->_next;
		alarm->_alarm_reset();
		return;
	}

	/* find predecessor in alarm queue */
	Alarm *curr;
	for (curr = _active_head; curr && (curr->_next != alarm); curr = curr->_next);

	/* alarm is not enqueued */
	if (!curr) return;

	/* remove alarm from alarm queue */
	curr->_next = alarm->_next;
	alarm->_alarm_reset();
}


Timeout::Alarm *Alarm_timeout_scheduler::_alarm_get_pending_alarm()
{
	Mutex::Guard mutex_guard(_mutex);

	do {
		if (!_active_head || !_active_head->_raw.is_pending_at(_now, _now_period)) {
			return nullptr; }

		/* remove alarm from head of the list */
		Alarm *pending_alarm = _active_head;
		_active_head = _active_head->_next;

		/*
		 * Acquire dispatch mutex to defer destruction until the call of '_on_alarm'
		 * is finished
		 */
		pending_alarm->_dispatch_mutex.acquire();

		/* reset alarm object */
		pending_alarm->_next = nullptr;
		pending_alarm->_active--;

		if (pending_alarm->_delete) {
			pending_alarm->_dispatch_mutex.release();
			continue;
		}
		return pending_alarm;
	} while (true);
}


void Alarm_timeout_scheduler::_alarm_handle(Alarm::Time curr_time)
{
	/*
	 * Raise the time counter and if it wraps, update also in which
	 * period of the time counter we are.
	 */
	if (_now > curr_time) {
		_now_period = !_now_period;
	}
	_now = curr_time;

	if (!_min_handle_period.is_pending_at(_now, _now_period)) {
		return;
	}
	Alarm::Time const deadline         = _now + _min_handle_period.period;
	_min_handle_period.deadline        = deadline;
	_min_handle_period.deadline_period = _now > deadline ?
	                                     !_now_period : _now_period;

	/*
	 * Dequeue all pending alarms before starting to re-schedule. Otherwise,
	 * a long-lasting alarm that has a deadline in the next now_period might
	 * get scheduled as head of this now_period falsely because the code
	 * thinks that it belongs to the last now_period.
	 */
	while (Alarm *curr = _alarm_get_pending_alarm()) {

		/* enqueue alarm into list of pending alarms */
		curr->_next = _pending_head;
		_pending_head = curr;
	}
	while (Alarm *curr = _pending_head) {

		/* dequeue alarm from list of pending alarms */
		_pending_head = _pending_head->_next;
		curr->_next = nullptr;

		uint64_t triggered = 1;

		if (curr->_raw.period) {
			Alarm::Time deadline = curr->_raw.deadline;

			/* schedule next event */
			if (deadline == 0)
				 deadline = curr_time;

			triggered += (curr_time - deadline) / curr->_raw.period;
		}

		/* do not reschedule if alarm function returns 0 */
		bool reschedule = curr->_on_alarm(triggered);

		if (reschedule) {

			/*
			 * At this point, the alarm deadline normally is somewhere near
			 * the current time but If the alarm had no deadline by now,
			 * initialize it with the current time.
			 */
			if (curr->_raw.deadline == 0) {
				curr->_raw.deadline        = _now;
				curr->_raw.deadline_period = _now_period;
			}
			/*
			 * Raise the deadline value by one period of the alarm and
			 * if the deadline value wraps thereby, update also in which
			 * period it is located.
			 */
			Alarm::Time const deadline = curr->_raw.deadline +
			                             triggered * curr->_raw.period;
			if (curr->_raw.deadline > deadline) {
				curr->_raw.deadline_period = !curr->_raw.deadline_period;
			}
			curr->_raw.deadline = deadline;

			/* synchronize enqueue operation */
			Mutex::Guard mutex_guard(_mutex);
			_alarm_unsynchronized_enqueue(curr);
		}

		/* release alarm, resume concurrent destructor operation */
		curr->_dispatch_mutex.release();
	}
}


void Alarm_timeout_scheduler::_alarm_setup_alarm(Alarm &alarm, Alarm::Time period, Alarm::Time first_duration)
{
	/*
	 * If the alarm is already present in the queue, re-consider its queue
	 * position because its deadline might have changed. I.e., if an alarm is
	 * rescheduled with a new timeout before the original timeout triggered.
	 */
	if (alarm._active)
		_alarm_unsynchronized_dequeue(&alarm);

	Alarm::Time deadline = _now + first_duration;
	alarm._alarm_assign(period, deadline, _now > deadline ? !_now_period : _now_period, this);

	_alarm_unsynchronized_enqueue(&alarm);
}


void Alarm_timeout_scheduler::_alarm_schedule_absolute(Alarm *alarm, Alarm::Time duration)
{
	Mutex::Guard alarm_list_guard(_mutex);

	_alarm_setup_alarm(*alarm, 0, duration);
}


void Alarm_timeout_scheduler::_alarm_schedule(Alarm *alarm, Alarm::Time period)
{
	Mutex::Guard alarm_list_guard(_mutex);

	/*
	 * Refuse to schedule a periodic timeout of 0 because it would trigger
	 * infinitely in the 'handle' function. To account for the case where the
	 * alarm object was already scheduled, we make sure to remove it from the
	 * queue.
	 */
	if (period == 0) {
		_alarm_unsynchronized_dequeue(alarm);
		return;
	}

	/* first deadline is overdue */
	_alarm_setup_alarm(*alarm, period, 0);
}


void Alarm_timeout_scheduler::_alarm_discard(Alarm *alarm)
{
	/*
	 * Make sure that nobody is inside the '_alarm_get_pending_alarm' when
	 * grabbing the '_dispatch_mutex'. This is important when this function
	 * is called from the 'Alarm' destructor. Without the '_dispatch_mutex',
	 * we could take the mutex and proceed with destruction just before
	 * '_alarm_get_pending_alarm' tries to grab the mutex. When the destructor
	 * is finished, '_alarm_get_pending_alarm' would proceed with operating on
	 * a dangling pointer.
	 */
	if (alarm) {
		{
			/* inform that this object is going to be deleted */
			Mutex::Guard alarm_guard(alarm->_dispatch_mutex);
			alarm->_delete = true;
		}
		{
			Mutex::Guard alarm_list_guard(_mutex);
			_alarm_unsynchronized_dequeue(alarm);
		}

		/* get anyone using this out of '_alarm_get_pending_alarm'() finally */
		Mutex::Guard alarm_guard(alarm->_dispatch_mutex);
	}
}


bool Alarm_timeout_scheduler::_alarm_next_deadline(Alarm::Time *deadline)
{
	Mutex::Guard alarm_list_guard(_mutex);

	if (!_active_head) return false;

	if (deadline)
		*deadline = _active_head->_raw.deadline;

	if (*deadline < _min_handle_period.deadline) {
		*deadline = _min_handle_period.deadline;
	}
	return true;
}
