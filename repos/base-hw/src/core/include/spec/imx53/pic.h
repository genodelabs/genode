/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PIC_H_
#define _PIC_H_

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <board.h>

namespace Genode
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Pic;
}

class Genode::Pic : public Mmio
{
	public:

		enum { NR_OF_IRQ = 109 };

	protected:

		/**
		 * Interrupt control register
		 */
		struct Intctrl : Register<0, 32>
		{
			struct Enable    : Bitfield<0,1>  { };
			struct Nsen      : Bitfield<16,1> { };
			struct Nsen_mask : Bitfield<31,1> { };
		};

		struct Priomask : Register<0xc, 32>
		{
			struct Mask : Bitfield<0,8> { };
		};

		struct Syncctrl : Register<0x10, 32>
		{
			struct Syncmode : Bitfield<0,2> { };
		};

		struct Dsmint : Register<0x14, 32>
		{
			struct Dsm : Bitfield<0,1> { };
		};

		/**
		 * Interrupt security registers
		 */
		struct Intsec : Register_array<0x80, 32, NR_OF_IRQ, 1>
		{
			struct Nonsecure : Bitfield<0, 1> { };
		};

		/**
		 * Interrupt set enable registers
		 */
		struct Enset : Register_array<0x100, 32, NR_OF_IRQ, 1, true>
		{
			struct Set_enable : Bitfield<0, 1> { };
		};

		/**
		 * Interrupt clear enable registers
		 */
		struct Enclear : Register_array<0x180, 32, NR_OF_IRQ, 1, true>
		{
			struct Clear_enable : Bitfield<0, 1> { };
		};

		/**
		 * Interrupt priority level registers
		 */
		struct Priority  : Register_array<0x400, 32, NR_OF_IRQ, 8>
		{
			enum { MIN_PRIO = 0xff };
		};

		/**
		 * Pending registers
		 */
		struct Pndr  : Register_array<0xd00, 32, NR_OF_IRQ, 1>
		{
			struct Pending : Bitfield<0, 1> { };
		};

		/**
		 * Highest interrupt pending registers
		 */
		struct Hipndr  : Register_array<0xd80, 32, NR_OF_IRQ, 1, true>
		{
			struct Pending : Bitfield<0, 1> { };
		};

		/**
		 * Maximum supported interrupt priority
		 */
		unsigned _max_priority() { return 255; }

		/**
		 * Initializes security extension if needed
		 */
		void _init_security_ext();

	public:

		/**
		 * Constructor
		 */
		Pic() : Mmio(Board::TZIC_MMIO_BASE)
		{
			for (unsigned i = 0; i < NR_OF_IRQ; i++) {
				write<Intsec::Nonsecure>(1, i);
				write<Enclear::Clear_enable>(1, i);
			}
			write<Priomask::Mask>(0x1f);
			write<Intctrl>(Intctrl::Enable::bits(1) |
			               Intctrl::Nsen::bits(1)   |
			               Intctrl::Nsen_mask::bits(1));
			_init_security_ext();
		}

		/**
		 * Mark interrupt unsecure
		 *
		 * \param i  targeted interrupt
		 */
		void unsecure(unsigned const i);

		/**
		 * Mark interrupt secure
		 *
		 * \param i  targeted interrupt
		 */
		void secure(unsigned const i);

		/**
		 * Initialize processor local interface of the controller
		 */
		void init_processor_local() { }

		/**
		 * Receive a pending request number 'i'
		 */
		bool take_request(unsigned & i)
		{
			for (unsigned j = 0; j < NR_OF_IRQ; j++) {
				if (read<Hipndr::Pending>(j)) {
					i = j;
					return true;
				}
			}
			return false;
		}

		/**
		 * Finish the last taken request
		 */
		void finish_request() { }

		/**
		 * Validate request number 'i'
		 */
		bool valid(unsigned const i) const {
			return i < NR_OF_IRQ; }

		/**
		 * Mask all interrupts
		 */
		void mask()
		{
			for (unsigned i=0; i < NR_OF_IRQ; i++)
				write<Enclear::Clear_enable>(1, i);
		}

		/**
		 * Unmask interrupt
		 *
		 * \param interrupt_id  kernel name of targeted interrupt
		 */
		void unmask(unsigned const interrupt_id, unsigned)
		{
			if (interrupt_id < NR_OF_IRQ) {
				write<Enset::Set_enable>(1, interrupt_id);
			}
		}

		/**
		 * Mask interrupt 'i'
		 */
		void mask(unsigned const i)
		{
			if (i < NR_OF_IRQ)
				write<Enclear::Clear_enable>(1, i);
		}

		/**
		 * Wether an interrupt is inter-processor interrupt of a processor
		 *
		 * \param interrupt_id  kernel name of the interrupt
		 * \param processor_id  kernel name of the processor
		 */
		bool is_ip_interrupt(unsigned const interrupt_id,
		                     unsigned const processor_id)
		{
			return false;
		}

		/**
		 * Trigger the inter-processor interrupt of a processor
		 *
		 * \param processor_id  kernel name of the processor
		 */
		void trigger_ip_interrupt(unsigned const processor_id) { }
};

namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _PIC_H_ */
