/*
 * \brief  Lx_kit timeout backend
 * \author Stefan Kalkowski
 * \date   2021-05-05
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_kit/env.h>

void Lx_kit::Timeout::start(unsigned long us)
{
	_timeout.schedule(Microseconds(us));
}


void Lx_kit::Timeout::stop()
{
	_timeout.discard();
}


void Lx_kit::Timeout::_handle(Genode::Duration)
{
	_scheduler.unblock_time_handler();
	_scheduler.schedule();
}


Lx_kit::Timeout::Timeout(Timer::Connection & timer,
                         Scheduler & scheduler)
:
	_scheduler(scheduler),
	_timeout(timer, *this, &Timeout::_handle) { }
