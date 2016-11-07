/*
 * \brief  Programmable interrupt controller for core
 * \author Reto Buerki
 * \date   2015-02-17
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SPEC__X86_64__PIC_H_
#define _CORE__INCLUDE__SPEC__X86_64__PIC_H_

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

		uint8_t _irt_count;

		enum {
			/* Number of Redirection Table entries */
			IRTE_COUNT = 24,

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
		void _update_irt_entry(unsigned irq)
		{
			Irte::access_t irte;

			write<Ioregsel>(IOREDTBL + 2 * irq);
			irte = read<Iowin>();

			Irte::Pol::set(irte, _irq_mode[irq].polarity);
			Irte::Trg::set(irte, _irq_mode[irq].trigger_mode);

			write<Ioregsel>(IOREDTBL + 2 * irq);
			write<Iowin>(irte);
		}

		/**
		 * Create redirection table entry for given IRQ
		 */
		Irte::access_t _create_irt_entry(unsigned const irq)
		{
			Irte::access_t irte = REMAP_BASE + irq;
			Irte::Mask::set(irte, 1);

			Irte::Pol::set(irte, _irq_mode[irq].polarity);
			Irte::Trg::set(irte, _irq_mode[irq].trigger_mode);

			return irte;
		}

	public:

		Ioapic() : Mmio(Board::MMIO_IOAPIC_BASE)
		{
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

				/* remap all IRQs managed by I/O APIC */
				if (i < IRTE_COUNT) {
					Irte::access_t irte = _create_irt_entry(i);
					write<Ioregsel>(IOREDTBL + 2 * i + 1);
					write<Iowin>(irte >> Iowin::ACCESS_WIDTH);
					write<Ioregsel>(IOREDTBL + 2 * i);
					write<Iowin>(irte);
				}
			}
		};

		/**
		 * Set/unset mask bit of IRTE for given vector
		 *
		 * \param vector  targeted vector
		 * \param set     whether to set or to unset the mask bit
		 */
		void toggle_mask(unsigned const vector, bool const set)
		{
			/*
			 * Ignore toggle requests for vectors not handled by the I/O APIC.
			 */
			if (vector < REMAP_BASE || vector >= REMAP_BASE + IRTE_COUNT) {
				return;
			}

			const unsigned irq = vector - REMAP_BASE;

			/*
			 * Only mask existing RTEs and do *not* mask edge-triggered
			 * interrupts to avoid losing them while masked, see Intel
			 * 82093AA I/O Advanced Programmable Interrupt Controller
			 * (IOAPIC) specification, section 3.4.2, "Interrupt Mask"
			 * flag and edge-triggered interrupts or:
			 * http://yarchive.net/comp/linux/edge_triggered_interrupts.html
			 */
			if (_edge_triggered(irq) && set) { return; }

			write<Ioregsel>(IOREDTBL + (2 * irq));
			Irte::access_t irte = read<Iowin>();
			Irte::Mask::set(irte, set);
			write<Iowin>(irte);
		}

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
		struct Iowin    : Register<0x10, 32> { };
};

class Genode::Pic : public Mmio
{
	private:

		/*
		 * Registers
		 */

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

		/**
		 * Determine lowest pending interrupt in ISR register
		 *
		 * \return index of first ISR bit set starting at index one, zero if no
		 *         bit is set.
		 */
		inline unsigned get_lowest_bit(void)
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

		Ioapic ioapic;

		bool take_request(unsigned &irq);

		void finish_request();

		void unmask(unsigned const i, unsigned);

		void mask(unsigned const i);

		/*
		 * Dummies
		 */

		bool is_ip_interrupt(unsigned, unsigned) { return false; }
		void trigger_ip_interrupt(unsigned) { }
};

namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _CORE__INCLUDE__SPEC__X86_64__PIC_H_ */
