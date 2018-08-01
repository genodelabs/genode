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

/* core includes */
#include <board.h>

namespace Genode
{
	/*
	 * Redirection table entry
	 */
	struct Irte;

	/**
	 * IO advanced programmable interrupt controller
	 */
	class Ioapic;

	/**
	 * Programmable interrupt controller for core
	 */
	class Pic;

	enum { IRQ_COUNT = 256 };
}

struct Genode::Irte : Register<64>
{
	struct Pol  : Bitfield<13, 1> { };
	struct Trg  : Bitfield<15, 1> { };
	struct Mask : Bitfield<16, 1> { };
};

class Genode::Ioapic : public Mmio
{
	private:

		enum { REMAP_BASE = Board::VECTOR_REMAP_BASE };

		/* Number of Redirection Table entries */
		unsigned _irte_count = 0;

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

		static Irq_mode _irq_mode[IRQ_COUNT];

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

		Ioapic();

		/**
		 * Set/unset mask bit of IRTE for given vector
		 *
		 * \param vector  targeted vector
		 * \param set     whether to set or to unset the mask bit
		 */
		void toggle_mask(unsigned const vector, bool const set);

		/**
		 * Setup mode of an IRQ to specified trigger mode and polarity
		 *
		 * \param irq_number  ID of targeted interrupt
		 * \param trigger     new interrupt trigger mode
		 * \param polarity    new interrupt polarity setting
		 */
		void setup_irq_mode(unsigned irq_number, unsigned trigger,
		                    unsigned polarity);

		/*
		 * Registers
		 */

		struct Ioregsel : Register<0x00, 32> { };
		struct Iowin    : Register<0x10, 32>
		{
		    struct Maximum_redirection_entry : Bitfield<16, 8> { };
		};
};

class Genode::Pic : public Mmio
{
	private:

		/*
		 * Registers
		 */

		struct Id  : Register<0x020, 32> { };
		struct EOI : Register<0x0b0, 32, true> { };
		struct Svr : Register<0x0f0, 32>
		{
			struct APIC_enable : Bitfield<8, 1> { };
		};

		/*
		 * ISR register, see Intel SDM Vol. 3A, section 10.8.4.
		 *
		 * Each of the 8 32-bit ISR values is followed by 12 bytes of padding.
		 */
		struct Isr : Register_array<0x100, 32, 8 * 4, 32> { };

		/*
		 * Interrupt control register
		 */
		struct Icr_low  : Register<0x300, 32, true> {
			struct Vector          : Bitfield< 0, 8> { };
			struct Delivery_status : Bitfield<12, 1> { };
			struct Level_assert    : Bitfield<14, 1> { };
		};
		struct Icr_high : Register<0x310, 32, true> {
			struct Destination : Bitfield<24, 8> { };
		};

		/**
		 * Determine lowest pending interrupt in ISR register
		 *
		 * \return index of first ISR bit set starting at index one, zero if no
		 *         bit is set.
		 */
		inline unsigned get_lowest_bit(void);

		/**
		 * Mapping of our logical boot CPUs to the local APIC IDs
		 */
		static uint8_t lapic_ids[NR_OF_CPUS];

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
		Pic();

		Ioapic ioapic { };

		bool take_request(unsigned &irq);

		void finish_request();

		void unmask(unsigned const i, unsigned);

		void mask(unsigned const i);

		void store_apic_id(unsigned const cpu_id) {
			Id::access_t const lapic_id = read<Id>();
			lapic_ids[cpu_id] = (lapic_id >> 24) & 0xff;
		}

		void send_ipi(unsigned const);
};

namespace Kernel { using Genode::Pic; }

#endif /* _CORE__SPEC__X86_64__PIC_H_ */
