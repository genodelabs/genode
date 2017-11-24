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

Microseconds Timer::Time_source::max_timeout() const {
	return Microseconds(_epit.tics_to_us(~0U)); }

void Timer::Time_source::schedule_timeout(Microseconds     duration,
                                          Timeout_handler &handler)
{
	/*
	 * Program max timeout in case of duration 0 to avoid lost of accuracy
	 * due to wraps when value is chosen too small. Send instead a signal
	 * manually at end of this method.
	 */
	unsigned const tics = _epit.us_to_tics(duration.value ? duration.value
	                                                      : max_timeout().value);

	_handler = &handler;

	_timer_irq.ack_irq();

	_epit.start_one_shot(tics);

	/* trigger for a timeout 0 immediately the signal */
	if (!duration.value)
		Signal_transmitter(_signal_handler).submit();
}


Duration Timer::Time_source::curr_time()
{
	/* read EPIT status */
	bool           wrapped   = false;
	unsigned const max_value = _epit.current_max_value();
	unsigned const tic_value = _epit.value(wrapped);
	unsigned       passed_tics = 0;

	if (_irq && wrapped)
		passed_tics += max_value;

	passed_tics += max_value - tic_value;

	if (_irq || _epit.tics_to_us(passed_tics) > 1000)
		_curr_time_us += _epit.tics_to_us(passed_tics);

	return Duration(Microseconds(_curr_time_us));
}
