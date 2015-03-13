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
	_ioapic.toggle_mask(i, false);
}

void Pic::mask(unsigned const i)
{
	_ioapic.toggle_mask(i, true);
}
