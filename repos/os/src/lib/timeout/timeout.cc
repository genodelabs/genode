/*
 * \brief  Multiplexing one time source amongst different timeout subjects
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/timeout.h>

using namespace Genode;


/*************
 ** Timeout **
 *************/

void Timeout::schedule_periodic(Microseconds duration, Handler &handler)
{
	_alarm.handler = &handler;
	_alarm.periodic = true;
	_alarm.timeout_scheduler.schedule_periodic(*this, duration);
}

void Timeout::schedule_one_shot(Microseconds duration, Handler &handler)
{
	_alarm.handler = &handler;
	_alarm.periodic = false;
	_alarm.timeout_scheduler.schedule_one_shot(*this, duration);
}


/********************
 ** Timeout::Alarm **
 ********************/

bool Timeout::Alarm::on_alarm(unsigned)
{
	if (handler) {
		handler->handle_timeout(timeout_scheduler.curr_time()); }
	return periodic;
}



/*****************************
 ** Alarm_timeout_scheduler **
 *****************************/

void Alarm_timeout_scheduler::handle_timeout(Microseconds curr_time)
{
	_alarm_scheduler.handle(curr_time.value);

	unsigned long sleep_time_us;
	Alarm::Time deadline_us;
	if (_alarm_scheduler.next_deadline(&deadline_us)) {
		sleep_time_us = deadline_us - curr_time.value;
	} else {
		sleep_time_us = _time_source.max_timeout().value; }

	if (sleep_time_us == 0) {
		sleep_time_us = 1; }

	_time_source.schedule_timeout(Microseconds(sleep_time_us), *this);
}


Alarm_timeout_scheduler::Alarm_timeout_scheduler(Time_source &time_source)
:
	_time_source(time_source)
{
	time_source.schedule_timeout(Microseconds(0), *this);
}


void Alarm_timeout_scheduler::schedule_one_shot(Timeout      &timeout,
                                                Microseconds  duration)
{
	_alarm_scheduler.schedule_absolute(&timeout._alarm,
	                                   _time_source.curr_time().value +
	                                   duration.value);

	if (_alarm_scheduler.head_timeout(&timeout._alarm)) {
		_time_source.schedule_timeout(Microseconds(0), *this); }
}


void Alarm_timeout_scheduler::schedule_periodic(Timeout      &timeout,
                                                Microseconds  duration)
{
	_alarm_scheduler.handle(_time_source.curr_time().value);
	_alarm_scheduler.schedule(&timeout._alarm, duration.value);
	if (_alarm_scheduler.head_timeout(&timeout._alarm)) {
		_time_source.schedule_timeout(Microseconds(0), *this); }
}
