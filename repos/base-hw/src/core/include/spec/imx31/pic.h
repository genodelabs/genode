/*
 * \brief  Programmable interrupt controller for core
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PIC_H_
#define _PIC_H_

namespace Genode
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Pic;
}

class Genode::Pic : public Mmio
{
	private:

		struct Intcntl    : Register<0x0,  32> { }; /* IRQ control register */
		struct Nimask     : Register<0x4,  32> { }; /* normal IRQ mask reg. */
		struct Intennum   : Register<0x8,  32> { }; /* IRQ enable nr. reg. */
		struct Intdisnum  : Register<0xc,  32> { }; /* IRQ disable nr. reg. */
		struct Intenableh : Register<0x10, 32> { }; /* IRQ enable register */
		struct Intenablel : Register<0x14, 32> { }; /* IRQ enable register */
		struct Inttypeh   : Register<0x18, 32> { }; /* IRQ type register */
		struct Inttypel   : Register<0x1c, 32> { }; /* IRQ type register */
		struct Intsrch    : Register<0x48, 32> { }; /* IRQ source register */
		struct Intsrcl    : Register<0x4c, 32> { }; /* IRQ source register */
		struct Nipndh     : Register<0x58, 32> { }; /* normal IRQ pending */
		struct Nipndl     : Register<0x5c, 32> { }; /* normal IRQ pending */

		/**
		 * Normal interrupt priority registers
		 */
		struct Nipriority : Register_array<0x20, 32, 8, 32> { };

		/**
		 * Normal interrupt vector and status register
		 */
		struct Nivecsr : Register<0x40, 32>
		{
			struct Nvector : Bitfield<16, 16> { };
		};

		/**
		 * Validate request number 'i'
		 */
		bool _valid(unsigned const i) const { return i < NR_OF_IRQ; }

	public:

		enum { NR_OF_IRQ = 64 };

		/**
		 * Constructor
		 */
		Pic() : Mmio(Board::AVIC_MMIO_BASE)
		{
			write<Intenablel>(0);
			write<Intenableh>(0);
			write<Nimask>(~0);
			write<Intcntl>(0);
			write<Inttypeh>(0);
			write<Inttypel>(0);
			for (unsigned i = 0; i < Nipriority::ITEMS; i++) {
				write<Nipriority>(0, i); }
		}

		/**
		 * Try to receive an IRQ in 'i' and return wether it was successful
		 */
		bool take_request(unsigned & i)
		{
			i = read<Nivecsr::Nvector>();
			return _valid(i);
		}

		/**
		 * Unmask IRQ 'i'
		 */
		void unmask(unsigned const i, unsigned) {
			if (_valid(i)) { write<Intennum>(i); } }

		/**
		 * Mask IRQ 'i'
		 */
		void mask(unsigned const i) { if (i < NR_OF_IRQ) write<Intdisnum>(i); }

		/**
		 * Return wether IRQ 'irq_id' is inter-processor IRQ of CPU 'cpu_id'
		 */
		bool is_ip_interrupt(unsigned, unsigned) { return false; }

		/*************
		 ** Dummies **
		 *************/

		void init_processor_local() { }
		void trigger_ip_interrupt(unsigned) { }
		void finish_request() { /* done by source retraction or masking */ }
};

namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _PIC_H_ */
