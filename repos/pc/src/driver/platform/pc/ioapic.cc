/*
 * \brief  IOAPIC implementation
 * \author Johannes Schlatow
 * \date   2024-03-20
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <ioapic.h>

unsigned Driver::Ioapic::_read_max_entries()
{
	write<Ioregsel>(Ioregsel::IOAPICVER);
	return read<Iowin::Maximum_entries>() + 1;
}


bool Driver::Ioapic::handles_irq(unsigned irq)
{
	if (irq < _irq_start || irq >= _irq_start + _max_entries)
		return false;

	return true;
}


/**
 * Sets remapping bit and destination index in IOAPIC redirection table.
 *
 * Note: Expected to be called only if handles_irq() returned true.
 */
void Driver::Ioapic::remap_irq(unsigned from, unsigned to)
{
	const unsigned idx = from - _irq_start;

	/* read upper 32 bit */
	write<Ioregsel>(Ioregsel::IOREDTBL + 2 * idx + 1);
	Irte::access_t irte { read<Iowin>() };
	irte <<= 32;

	/* read lower 32 bit */
	write<Ioregsel>(Ioregsel::IOREDTBL + 2 * idx);
	irte |= read<Iowin>();

	/* remap entry */
	Irte::Remap::set(irte, 1);
	Irte::Index::set(irte, to & 0xFF);

	/* write upper 32 bit */
	write<Ioregsel>(Ioregsel::IOREDTBL + 2 * idx + 1);
	write<Iowin>((Iowin::access_t)(irte >> 32));

	/* write lower 32 bit */
	write<Ioregsel>(Ioregsel::IOREDTBL + 2 * idx);
	write<Iowin>((Iowin::access_t)(irte & 0xFFFFFFFF));
}


/**
 * Reads and returns IRQ configuration from IOAPIC redirection table.
 *
 * Note: Expected to be called only if handles_irq() returned true.
 */
Driver::Ioapic::Irq_config Driver::Ioapic::irq_config(unsigned irq)
{
	const unsigned idx = irq - _irq_start;

	/* read upper 32 bit */
	write<Ioregsel>(Ioregsel::IOREDTBL + 2 * idx + 1);
	Irte::access_t irte { read<Iowin>() };
	irte <<= 32;

	/* read lower 32 bit */
	write<Ioregsel>(Ioregsel::IOREDTBL + 2 * idx);
	irte |= read<Iowin>();

	/* extract trigger mode */
	Irq_session::Trigger trigger { Irq_session::TRIGGER_UNCHANGED };
	switch (Irte::Trigger_mode::get(irte)) {
	case Irte::Trigger_mode::EDGE:
		trigger = Irq_session::TRIGGER_EDGE;
		break;
	case Irte::Trigger_mode::LEVEL:
		trigger = Irq_session::TRIGGER_LEVEL;
		break;
	}

	/* extract destination mode */
	Irq_config::Mode mode { Irq_config::Mode::INVALID };
	switch (Irte::Destination_mode::get(irte)) {
	case Irte::Destination_mode::PHYSICAL:
		mode = Irq_config::Mode::PHYSICAL;
		break;
	case Irte::Destination_mode::LOGICAL:
		mode = Irq_config::Mode::LOGICAL;
		break;
	}

	return { mode, trigger,
	         Irte::Vector::get(irte),
	         Irte::Destination::get(irte) };
}
