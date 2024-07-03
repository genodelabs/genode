/*
 * \brief  Programmable interrupt controller for core
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

#ifndef _CORE__SPEC__X86_64__PIC_H_
#define _CORE__SPEC__X86_64__PIC_H_

/* Genode includes */
#include <util/mmio.h>
#include <hw/spec/x86_64/apic.h>
#include <hw/spec/x86_64/x86_64.h>

namespace Board {

	/*
	 * Redirection table entry
	 */
	struct Irte;

	/**
	 * IO advanced programmable interrupt controller
	 */
	class Global_interrupt_controller;

	/**
	 * Programmable interrupt controller for core
	 */
	class Local_interrupt_controller;

	enum { IRQ_COUNT = 256 };
}


struct Board::Irte : Genode::Register<64>
{
	struct Pol  : Bitfield<13, 1> { };
	struct Trg  : Bitfield<15, 1> { };
	struct Mask : Bitfield<16, 1> { };
};


class Board::Global_interrupt_controller : public Genode::Mmio<Hw::Cpu_memory_map::MMIO_IOAPIC_SIZE>
{
	private:

		using uint8_t = Genode::uint8_t;

		enum {
			/* Register selectors */
			IOAPICVER = 0x01,
			IOREDTBL  = 0x10,

			/* IRQ modes */
			TRIGGER_EDGE  = 0,
			TRIGGER_LEVEL = 1,
			POLARITY_HIGH = 0,
			POLARITY_LOW  = 1,
		};

		/**
		 * IRQ mode specifies trigger mode and polarity of an IRQ
		 */
		struct Irq_mode
		{
			unsigned trigger_mode;
			unsigned polarity;
		};

		/*
		 * Registers
		 */

		struct Ioregsel : Register<0x00, 32> { };
		struct Iowin    : Register<0x10, 32>
		{
		    struct Maximum_redirection_entry : Bitfield<16, 8> { };
		};

		unsigned _irte_count = 0;       /* number of redirection table entries */
		Irq_mode _irq_mode[IRQ_COUNT];

		/**
		 * Return whether 'irq' is an edge-triggered interrupt
		 */
		bool _edge_triggered(unsigned const irq)
		{
			return _irq_mode[irq].trigger_mode == TRIGGER_EDGE;
		}

		/**
		 * Update IRT entry of given IRQ
		 *
		 * Note: The polarity and trigger flags are located in the lower
		 *       32 bits so only the necessary half of the IRT entry is
		 *       updated.
		 */
		void _update_irt_entry(unsigned irq);

		/**
		 * Create redirection table entry for given IRQ
		 */
		Irte::access_t _create_irt_entry(unsigned const irq);

	public:

		Global_interrupt_controller();

		void init();

		/**
		 * Set/unset mask bit of IRTE for given vector
		 *
		 * \param vector  targeted vector
		 * \param set     whether to set or to unset the mask bit
		 */
		void toggle_mask(unsigned const vector,
		                 bool     const set);

		/**
		 * Setup mode of an IRQ to specified trigger mode and polarity
		 *
		 * \param irq_number  ID of targeted interrupt
		 * \param trigger     new interrupt trigger mode
		 * \param polarity    new interrupt polarity setting
		 */
		void irq_mode(unsigned irq_number,
		              unsigned trigger,
		              unsigned polarity);
};


class Board::Local_interrupt_controller : private Hw::Local_apic
{
	private:

		Global_interrupt_controller &_global_irq_ctrl;

		/**
		 * Determine lowest pending interrupt in ISR register
		 *
		 * \return index of first ISR bit set starting at index one, zero if no
		 *         bit is set.
		 */
		inline unsigned get_lowest_bit(void);

	public:

		enum {
			/*
			 * FIXME: dummy ipi value on non-SMP platform, should be removed
			 *        when SMP is an aspect of CPUs only compiled where
			 *        necessary
			 */
			IPI       = 255,
			NR_OF_IRQ = IRQ_COUNT,
		};

		/**
		 * Constructor
		 */
		Local_interrupt_controller(Global_interrupt_controller &global_irq_ctrl);

		bool take_request(unsigned &irq);

		void finish_request();

		void unmask(unsigned const i, unsigned);

		void mask(unsigned const i);

		void irq_mode(unsigned irq, unsigned trigger, unsigned polarity);

		void send_ipi(unsigned const);

		void init();
};

#endif /* _CORE__SPEC__X86_64__PIC_H_ */
