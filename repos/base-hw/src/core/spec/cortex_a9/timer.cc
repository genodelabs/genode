/*
 * \brief   Timer implementation specific to Cortex A9
 * \author  Stefan Kalkowski
 * \date    2016-01-07
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>
#include <timer.h>

Genode::Timer::Timer()
: Genode::Mmio(Genode::Platform::mmio_to_virt(Board::PRIVATE_TIMER_MMIO_BASE))
{
	write<Control::Timer_enable>(0);
}


void Genode::Timer::start_one_shot(time_t const tics, unsigned const)
{
	enum { PRESCALER = Board::CORTEX_A9_PRIVATE_TIMER_DIV - 1 };

	/* reset timer */
	write<Interrupt_status::Event>(1);
	Control::access_t control = 0;
	Control::Irq_enable::set(control, 1);
	Control::Prescaler::set(control, PRESCALER);
	write<Control>(control);

	/* load timer and start decrementing */
	write<Load>(tics);
	write<Control::Timer_enable>(1);
}
