/*
 * \brief   Pic implementation specific to Rpi3
 * \author  Stefan Kalkowski
 * \date    2019-05-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <pic.h>
#include <platform.h>


Genode::Pic::Pic()
: Mmio(Platform::mmio_to_virt(Board::LOCAL_IRQ_CONTROLLER_BASE)) { }


bool Genode::Pic::take_request(unsigned & irq)
{
	Core0_irq_source::access_t src = read<Core0_irq_source>();
	if ((1 << TIMER_IRQ) & src) {
		irq = TIMER_IRQ;
		return true;
	}
	return false;
}


void Genode::Pic::mask() { }


void Genode::Pic::unmask(unsigned const i, unsigned cpu)
{
	if (cpu > 0)
		Genode::raw("multi-core irq controller not implemented yet");

	if (i == TIMER_IRQ) {
		write<Core0_timer_irq_control::Cnt_p_ns_irq>(1);
		return;
	}

	Genode::raw("irq of peripherals != timer not implemented yet!");
}


void Genode::Pic::mask(unsigned const i)
{
	if (i == TIMER_IRQ) {
		write<Core0_timer_irq_control::Cnt_p_ns_irq>(0);
		return;
	}

	Genode::raw("irq of peripherals != timer not implemented yet!");
}
