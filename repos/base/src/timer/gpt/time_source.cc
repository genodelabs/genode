/*
 * \brief  Time source that uses the General Purpose Timer (Freescale)
 * \author Stefan Kalkowski
 * \date   2019-04-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
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
	_handler = &handler;
	
	/* set to minimum ticks value to not miss a too short timeout */
	Genode::uint32_t const ticks =
		Genode::max(1UL, (duration.value * TICKS_PER_MS) / 1000);

	/* clear interrupts */
	if (read<Sr>()) {
		write<Sr>(0xffffffff);
		_timer_irq.ack_irq();
	}

	/* set new timeout */
	write<Ocr1>(read<Cnt>() + ticks);
}


Duration Timer::Time_source::curr_time()
{
	Cnt::access_t cur_cnt = read<Cnt>();
	Genode::Microseconds us(timer_ticks_to_us(cur_cnt - _last_cnt,
	                                          TICKS_PER_MS));
	_last_cnt = cur_cnt;
	_curr_time.add(us);
	return _curr_time;
}


Genode::Microseconds Timer::Time_source::max_timeout() const
{
	static unsigned long max = timer_ticks_to_us(0xffffffff, TICKS_PER_MS);
	return Genode::Microseconds(max);
}


void Timer::Time_source::_initialize()
{
	_timer_irq.sigh(_signal_handler);

	write<Cr>(0);
	write<Ir>(0);
	write<Ocr1>(0);
	write<Ocr2>(0);
	write<Ocr3>(0);
	write<Icr1>(0);
	write<Icr2>(0);
	write<Cr::Clk_src>(Cr::Clk_src::HIGH_FREQ_REF_CLK);
	while (read<Cr::Swr>()) ;
	write<Sr>(0);
	write<Cr::Frr>(1);
	write<Cr::En_mod>(1);
	write<Cr::En>(1);
	write<Ir>(1);
}
