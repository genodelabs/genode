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
	/* on duration 0 trigger directly at function end and set max timeout */
	unsigned long const us = duration.value ? duration.value
	                                        : max_timeout().value;
	unsigned long const ticks = (1ULL * us * TICKS_PER_MS) / 1000;

	_handler = &handler;
	_timer_irq.ack_irq();

	/* wait until ongoing reset operations are finished */
	while (read<Cr::Swr>()) ;

	/* disable timer */
	write<Cr::En>(0);

	/* clear interrupt */
	write<Sr::Ocif>(1);

	/* configure timer for a one-shot */
	write<Cr>(Cr::prepare_one_shot());
	write<Lr>(ticks);
	write<Cmpr>(0);

	/* start timer */
	write<Cr::En>(1);

	/* trigger for a timeout 0 immediately the signal */
	if (!duration.value) {
		Signal_transmitter(_signal_handler).submit();
	}
}


Duration Timer::Time_source::curr_time()
{
	/* read timer status */
	unsigned long      diff_ticks = 0;
	Lr::access_t const max_value  = read<Lr>();
	Cnt::access_t      cnt        = read<Cnt>();
	bool         const wrapped    = read<Sr::Ocif>();

	/* determine how many ticks have passed */
	if (_irq && wrapped) {
		cnt         = read<Cnt>();
		diff_ticks += max_value;
	}
	diff_ticks += max_value - cnt;
	unsigned long const diff_us = timer_ticks_to_us(diff_ticks, TICKS_PER_MS);

	/* update time only on IRQs and if rate is under 1000 per second */
	if (_irq || diff_us > 1000) {
		_curr_time.add(Genode::Microseconds(diff_us));
	}
	return _curr_time;
}
