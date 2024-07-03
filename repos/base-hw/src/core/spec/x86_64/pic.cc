/*
 * \brief  Programmable interrupt controller for core
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \author Alexander Boettcher
 * \date   2015-02-17
 */

/*
 * Copyright (C) 2015-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <irq_session/irq_session.h>

/* core includes */
#include <port_io.h>
#include <platform.h>

using namespace Core;
using namespace Board;

enum {
	REMAP_BASE      = Board::VECTOR_REMAP_BASE,
	PIC_CMD_MASTER  = 0x20,
	PIC_CMD_SLAVE   = 0xa0,
	PIC_DATA_MASTER = 0x21,
	PIC_DATA_SLAVE  = 0xa1,
};


/***************************************
 ** Board::Local_interrupt_controller **
 ***************************************/

Local_interrupt_controller::
Local_interrupt_controller(Global_interrupt_controller &global_irq_ctrl)
:
	Local_apic({Platform::mmio_to_virt(Hw::Cpu_memory_map::lapic_phys_base())}),
	_global_irq_ctrl { global_irq_ctrl }
{
	init();
}

void Local_interrupt_controller::init()
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


bool Local_interrupt_controller::take_request(unsigned &irq)
{
	irq = get_lowest_bit();
	if (!irq) {
		return false;
	}

	irq -= 1;
	return true;
}


void Local_interrupt_controller::finish_request()
{
	write<EOI>(0);
}


void Local_interrupt_controller::unmask(unsigned const i, unsigned)
{
	_global_irq_ctrl.toggle_mask(i, false);
}


void Local_interrupt_controller::mask(unsigned const i)
{
	_global_irq_ctrl.toggle_mask(i, true);
}


void Local_interrupt_controller::irq_mode(unsigned irq_number,
                                          unsigned trigger,
                                          unsigned polarity)
{
	_global_irq_ctrl.irq_mode(irq_number, trigger, polarity);
}


inline unsigned Local_interrupt_controller::get_lowest_bit()
{
	unsigned bit, vec_base = 0;

	for (unsigned i = 0; i < 8 * 4; i += 4) {
		bit = __builtin_ffs(read<Isr>(i));
		if (bit) {
			return vec_base + bit;
		}
		vec_base += 32;
	}
	return 0;
}


void Local_interrupt_controller::send_ipi(unsigned const cpu_id)
{
	while (read<Icr_low::Delivery_status>())
		asm volatile("pause" : : : "memory");

	Icr_high::access_t icr_high = 0;
	Icr_low::access_t  icr_low  = 0;

	Icr_high::Destination::set(icr_high, cpu_id);

	Icr_low::Vector::set(icr_low, Local_interrupt_controller::IPI);
	Icr_low::Level_assert::set(icr_low);

	/* program */
	write<Icr_high>(icr_high);
	write<Icr_low>(icr_low);
}


/****************************************
 ** Board::Global_interrupt_controller **
 ****************************************/

void Global_interrupt_controller::irq_mode(unsigned irq_number,
                                           unsigned trigger,
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


void Global_interrupt_controller::_update_irt_entry(unsigned irq)
{
	Irte::access_t irte;

	write<Ioregsel>(IOREDTBL + 2 * irq);
	irte = read<Iowin>();

	Irte::Pol::set(irte, _irq_mode[irq].polarity);
	Irte::Trg::set(irte, _irq_mode[irq].trigger_mode);

	write<Ioregsel>(IOREDTBL + 2 * irq);
	write<Iowin>((Iowin::access_t)(irte));
}


Irte::access_t
Global_interrupt_controller::_create_irt_entry(unsigned const irq)
{
	Irte::access_t irte = REMAP_BASE + irq;
	Irte::Mask::set(irte, 1);

	Irte::Pol::set(irte, _irq_mode[irq].polarity);
	Irte::Trg::set(irte, _irq_mode[irq].trigger_mode);

	return irte;
}


Global_interrupt_controller::Global_interrupt_controller()
:
	Mmio({(char *)Platform::mmio_to_virt(Hw::Cpu_memory_map::MMIO_IOAPIC_BASE), Mmio::SIZE})
{
	write<Ioregsel>(IOAPICVER);
	_irte_count = read<Iowin::Maximum_redirection_entry>() + 1;

	for (unsigned i = 0; i < IRQ_COUNT; i++)
	{
		/* set legacy/ISA IRQs to edge, high */
		if (i <= Board::ISA_IRQ_END) {
			_irq_mode[i].trigger_mode = TRIGGER_EDGE;
			_irq_mode[i].polarity = POLARITY_HIGH;
		} else {
			_irq_mode[i].trigger_mode = TRIGGER_LEVEL;
			_irq_mode[i].polarity = POLARITY_LOW;
		}
	}

	init();
}


void Global_interrupt_controller::init()
{
	/* remap all IRQs managed by I/O APIC */
	for (unsigned i = 0; i < _irte_count; i++)
	{
		Irte::access_t irte = _create_irt_entry(i);
		write<Ioregsel>(IOREDTBL + 2 * i + 1);
		write<Iowin>((Iowin::access_t)(irte >> Iowin::ACCESS_WIDTH));
		write<Ioregsel>(IOREDTBL + 2 * i);
		write<Iowin>((Iowin::access_t)(irte));
	}
}


void Global_interrupt_controller::toggle_mask(unsigned const vector,
                                              bool     const set)
{
	/*
	 * Ignore toggle requests for vectors not handled by the I/O APIC.
	 */
	if (vector < REMAP_BASE || vector >= REMAP_BASE + _irte_count)
		return;

	const unsigned irq = vector - REMAP_BASE;

	/*
	 * Only mask existing RTEs and do *not* mask edge-triggered
	 * interrupts to avoid losing them while masked, see Intel
	 * 82093AA I/O Advanced Programmable Interrupt Controller
	 * (IOAPIC) specification, section 3.4.2, "Interrupt Mask"
	 * flag and edge-triggered interrupts or:
	 * http://yarchive.net/comp/linux/edge_triggered_interrupts.html
	 */
	if (_edge_triggered(irq) && set)
		return;

	write<Ioregsel>(IOREDTBL + (2 * irq));
	Irte::access_t irte = read<Iowin>();
	Irte::Mask::set(irte, set);
	write<Iowin>((Iowin::access_t)(irte));
}
