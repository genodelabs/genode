/*
 * \brief  Time source that uses the Enhanced Periodic Interrupt Timer (Freescale)
 * \author Norman Feske
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \author Alexander Boettcher
 * \date   2009-06-16
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <time_source.h>

using namespace Genode;


void Timer::Time_source::schedule_timeout(Genode::Microseconds  duration,
                                          Timeout_handler      &handler)
{
	unsigned long const ticks = (1ULL * duration.value * TICKS_PER_MS) / 1000;
	_handler = &handler;
	_timer_irq.ack_irq();
	_cleared_ticks = 0;

	/* disable timer */
	write<Cr::En>(0);

	/* clear interrupt and install timeout */
	write<Sr::Ocif>(1);
	write<Cr>(Cr::prepare_one_shot());
	write<Cmpr>(Cnt::MAX - ticks);

	/* start timer */
	write<Cr::En>(1);
}


Duration Timer::Time_source::curr_time()
{
	unsigned long const uncleared_ticks = Cnt::MAX - read<Cnt>() - _cleared_ticks;
	unsigned long const uncleared_us    = timer_ticks_to_us(uncleared_ticks, TICKS_PER_MS);

	/* update time only on IRQs and if rate is under 1000 per second */
	if (_irq || uncleared_us > 1000) {
		_curr_time.add(Genode::Microseconds(uncleared_us));
		_cleared_ticks += uncleared_ticks;
	}
	return _curr_time;
}
