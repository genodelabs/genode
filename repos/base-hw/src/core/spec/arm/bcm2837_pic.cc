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

#include <board.h>
#include <cpu.h>
#include <platform.h>


Board::Pic::Pic()
: Genode::Mmio(Genode::Platform::mmio_to_virt(Board::LOCAL_IRQ_CONTROLLER_BASE)) { }


bool Board::Pic::take_request(unsigned & irq)
{
	unsigned cpu = Genode::Cpu::executing_id();
	Core_irq_source<0>::access_t src = 0;
	switch (cpu) {
	case 0: src = read<Core_irq_source<0>>(); break;
	case 1: src = read<Core_irq_source<1>>(); break;
	case 2: src = read<Core_irq_source<2>>(); break;
	case 3: src = read<Core_irq_source<3>>(); break;
	}

	if ((1 << TIMER_IRQ) & src) {
		irq = TIMER_IRQ;
		return true;
	}

	if (0xf0 & src) {
		irq = IPI;
		switch (cpu) {
		case 0: write<Core_mailbox_clear<0>>(1); break;
		case 1: write<Core_mailbox_clear<1>>(1); break;
		case 2: write<Core_mailbox_clear<2>>(1); break;
		case 3: write<Core_mailbox_clear<3>>(1); break;
		}
		return true;
	}

	return false;
}


void Board::Pic::_timer_irq(unsigned cpu, bool enable)
{
	unsigned v = enable ? 1 : 0;
	switch (cpu) {
		case 0:
			write<Core_timer_irq_control<0>::Cnt_p_ns_irq>(v);
			return;
		case 1:
			write<Core_timer_irq_control<1>::Cnt_p_ns_irq>(v);
			return;
		case 2:
			write<Core_timer_irq_control<2>::Cnt_p_ns_irq>(v);
			return;
		case 3:
			write<Core_timer_irq_control<3>::Cnt_p_ns_irq>(v);
			return;
		default: ;
	}
}


void Board::Pic::_ipi(unsigned cpu, bool enable)
{
	unsigned v = enable ? 1 : 0;
	switch (cpu) {
		case 0:
			write<Core_mailbox_irq_control<0>>(v);
			return;
		case 1:
			write<Core_mailbox_irq_control<1>>(v);
			return;
		case 2:
			write<Core_mailbox_irq_control<2>>(v);
			return;
		case 3:
			write<Core_mailbox_irq_control<3>>(v);
			return;
		default: ;
	}
}


void Board::Pic::unmask(unsigned const i, unsigned cpu)
{
	switch (i) {
		case TIMER_IRQ: _timer_irq(cpu, true); return;
		case IPI:       _ipi(cpu, true);       return;
	}

	Genode::raw("irq of peripherals != timer not implemented yet! (irq=", i, ")");
}


void Board::Pic::mask(unsigned const i)
{
	unsigned cpu = Genode::Cpu::executing_id();
	switch (i) {
		case TIMER_IRQ: _timer_irq(cpu, false); return;
		case IPI:       _ipi(cpu, false);       return;
	}

	Genode::raw("irq of peripherals != timer not implemented yet! (irq=", i, ")");
}


void Board::Pic::irq_mode(unsigned, unsigned, unsigned) { }


void Board::Pic::send_ipi(unsigned cpu_target)
{
	switch (cpu_target) {
	case 0: write<Core_mailbox_set<0>>(1); return;
	case 1: write<Core_mailbox_set<1>>(1); return;
	case 2: write<Core_mailbox_set<2>>(1); return;
	case 3: write<Core_mailbox_set<3>>(1); return;
	}
}
