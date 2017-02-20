/*
 * \brief  Implementation of linux/delay.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <timer_session/connection.h>

#include <lx_kit/env.h>
#include <lx_kit/timer.h>

/*
 * XXX  "We may consider to use the Lx::Timer instead of opening a dedicated
 *      timer session" which I tried during the deprecation warning removal
 *      but it did not work out. At least the intel_fb at that point got stuck
 *      because the workqueue task got mutex blocked.
 */
static Genode::Constructible<Timer::Connection> _delay_timer;

static inline void __delay_timer(unsigned long usecs)
{
	if (!_delay_timer.constructed()) {
		_delay_timer.construct(Lx_kit::env().env());
	}

	_delay_timer->usleep(usecs);
}

void udelay(unsigned long usecs) { __delay_timer(usecs); }


void msleep(unsigned int msecs)
{
	__delay_timer(1000 * msecs);
	Lx::timer_update_jiffies();
}


void mdelay(unsigned long msecs) { msleep(msecs); }
