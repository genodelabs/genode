/*
 * \brief   A timer manages a continuous time and timeouts on it
 * \author  Martin Stein
 * \date    2016-03-23
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Core includes */
#include <kernel/cpu.h>
#include <kernel/timer.h>
#include <kernel/configuration.h>
#include <hw/assert.h>

using namespace Kernel;


void Timer::Irq::occurred() { _cpu.scheduler().timeout(); }


Timer::Irq::Irq(unsigned id, Cpu &cpu)
: Kernel::Irq(id, cpu.irq_pool()), _cpu(cpu) {}


time_t Timer::timeout_max_us() const
{
	return ticks_to_us(_max_value());
}


void Timer::set_timeout(Timeout * const timeout, time_t const duration)
{
	/*
	 * Remove timeout if it is already in use. Timeouts may get overridden as
	 * result of an update.
	 */
	if (timeout->_listed) {
		_timeout_list.remove(timeout);
	} else {
		timeout->_listed = true; }

	/* set timeout parameters */
	timeout->_end = time() + duration;

	/*
	 * Insert timeout. Timeouts are ordered ascending according to their end
	 * time to be able to quickly determine the nearest timeout.
	 */
	Timeout * t1 = 0;
	for (Timeout * t2 = _timeout_list.first();
	     t2 && t2->_end < timeout->_end;
	     t1 = t2, t2 = t2->next()) { }

	_timeout_list.insert(timeout, t1);
}


time_t Timer::schedule_timeout()
{
	/* get the timeout with the nearest end time */
	Timeout * timeout = _timeout_list.first();
	assert(timeout);

	/* install timeout at timer hardware */
	time_t duration = _duration();
	_time          += duration;
	_last_timeout_duration = (timeout->_end > _time) ? timeout->_end - _time : 1;
	_start_one_shot(_last_timeout_duration);

	return duration;
}


void Timer::process_timeouts()
{
	/*
	 * Walk through timeouts until the first whose end time is in the future.
	 */
	time_t t = time();
	while (true) {
		Timeout * const timeout = _timeout_list.first();
		if (!timeout) { break; }
		if (timeout->_end > t) { break; }
		_timeout_list.remove(timeout);
		timeout->_listed = false;
		timeout->timeout_triggered();
	}
}


Timer::Timer(Cpu & cpu)
: _device(cpu.id()), _irq(interrupt_id(), cpu),
  _last_timeout_duration(_max_value())
{
	/*
	 * The timer frequency should allow a good accuracy on the smallest
	 * timeout syscall value (1 us).
	 */
	assert(ticks_to_us(1) < 1 || ticks_to_us(_max_value()) == _max_value());

	/*
	 * The maximum measurable timeout is also the maximum age of a timeout
	 * installed by the timeout syscall. The timeout-age syscall returns a
	 * bogus value for older timeouts. A user that awoke from waiting for a
	 * timeout might not be schedulable in the same super period anymore.
	 * However, if the user can't manage to read the timeout age during the
	 * next super period, it's a bad configuration or the users fault. That
	 * said, the maximum timeout should be at least two times the super
	 * period).
	 */
	assert(ticks_to_us(_max_value()) > 2 * cpu_quota_us);
}
