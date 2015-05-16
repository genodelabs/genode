/*
 * \brief  Programmable interrupt controller for core
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2015-02-17
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <irq_session/irq_session.h>

/* core includes */
#include <port_io.h>
#include "pic.h"

using namespace Genode;

enum {
	PIC_CMD_MASTER  = 0x20,
	PIC_CMD_SLAVE   = 0xa0,
	PIC_DATA_MASTER = 0x21,
	PIC_DATA_SLAVE  = 0xa1,
};

Pic::Pic() : Mmio(Board::MMIO_LAPIC_BASE)
{
	/* Start initialization sequence in cascade mode */
	outb(PIC_CMD_MASTER, 0x11);
	outb(PIC_CMD_SLAVE, 0x11);

	/* ICW2: Master PIC vector offset (32) */
	outb(PIC_DATA_MASTER, 0x20);
	/* ICW2: Slave PIC vector offset (40) */
	outb(PIC_DATA_SLAVE, 0x28);

	/* ICW3: Tell Master PIC that there is a slave PIC at IRQ2 */
	outb(PIC_DATA_MASTER, 4);

	/* ICW3: Tell Slave PIC its cascade identity */
	outb(PIC_DATA_SLAVE, 2);

	/* ICW4: Enable 8086 mode */
	outb(PIC_DATA_MASTER, 0x01);
	outb(PIC_DATA_SLAVE, 0x01);

	/* Disable legacy pic */
	outb(PIC_DATA_SLAVE,  0xff);
	outb(PIC_DATA_MASTER, 0xff);

	/* Set bit 8 of the APIC spurious vector register (SVR) */
	write<Svr::APIC_enable>(1);
}

bool Pic::take_request(unsigned &irq)
{
	irq = get_lowest_bit();
	if (!irq) {
		return false;
	}

	irq -= 1;
	return true;
}

void Pic::finish_request()
{
	write<EOI>(0);
}

void Pic::unmask(unsigned const i, unsigned)
{
	ioapic.toggle_mask(i, false);
}

void Pic::mask(unsigned const i)
{
	ioapic.toggle_mask(i, true);
}

Ioapic::Irq_mode Ioapic::_irq_mode[IRQ_COUNT];

void Ioapic::setup_irq_mode(unsigned irq_number, unsigned trigger,
                            unsigned polarity)
{
	const unsigned irq_nr = irq_number - REMAP_BASE;
	bool needs_sync = false;

	switch (trigger) {
	case Irq_session::TRIGGER_EDGE:
		_irq_mode[irq_nr].trigger_mode = TRIGGER_EDGE;
		needs_sync = true;
		break;
	case Irq_session::TRIGGER_LEVEL:
		_irq_mode[irq_nr].trigger_mode = TRIGGER_LEVEL;
		needs_sync = true;
		break;
	default:
		/* Do nothing */
		break;
	}

	switch (polarity) {
	case Irq_session::POLARITY_HIGH:
		_irq_mode[irq_nr].polarity = POLARITY_HIGH;
		needs_sync = true;
		break;
	case Irq_session::POLARITY_LOW:
		_irq_mode[irq_nr].polarity = POLARITY_LOW;
		needs_sync = true;
		break;
	default:
		/* Do nothing */
		break;
	}

	/* Update IR table if IRQ mode changed */
	if (needs_sync)
		_update_irt_entry(irq_nr);
}
