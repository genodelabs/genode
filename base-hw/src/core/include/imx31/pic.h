/*
 * \brief  Programmable interrupt controller for core
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-08-30
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__IMX31__PIC_H_
#define _INCLUDE__IMX31__PIC_H_

/* Genode includes */
#include <util/mmio.h>
#include <drivers/board.h>

namespace Imx31
{
	using namespace Genode;

	/**
	 * Programmable interrupt controller for core
	 */
	class Pic : public Mmio
	{
		/**
		 * Interrupt control register
		 */
		struct Intcntl : Register<0x0, 32>
		{
			struct Nm     : Bitfield<18,1> /* IRQ mode */
			{
				enum { SW_CONTROL = 0 };
			};

			struct Fiad   : Bitfield<19,1> { }; /* FIQ rises prio in arbiter */
			struct Niad   : Bitfield<20,1> { }; /* IRQ rises prio in arbiter */
			struct Fidis  : Bitfield<21,1> { }; /* FIQ disable */
			struct Nidis  : Bitfield<22,1> { }; /* IRQ disable */
			struct Abfen  : Bitfield<24,1> { }; /* if ABFLAG is sticky */
			struct Abflag : Bitfield<25,1> { }; /* rise prio in bus arbiter */

			static access_t init_value()
			{
				return Nm::bits(Nm::SW_CONTROL) |
				       Fiad::bits(0) |
				       Niad::bits(0) |
				       Fidis::bits(0) |
				       Nidis::bits(0) |
				       Abfen::bits(0) |
				       Abflag::bits(0);
			}
		};

		/**
		 * Normal interrupt mask register
		 */
		struct Nimask : Register<0x4, 32>
		{
			enum { NONE_MASKED = ~0 };
		};

		/**
		 * Interrupt enable number register
		 */
		struct Intennum : Register<0x8, 32>
		{
			struct Enable : Bitfield<0,6> { };
		};

		/**
		 * Interrupt disable number register
		 */
		struct Intdisnum : Register<0xc, 32>
		{
			struct Disable : Bitfield<0,6> { };
		};

		/**
		 * Interrupt enable register
		 */
		struct Intenableh : Register<0x10, 32> { };
		struct Intenablel : Register<0x14, 32> { };

		/**
		 * Interrupt type register
		 */
		struct Inttype { enum { ALL_IRQS = 0 }; };
		struct Inttypeh : Register<0x18, 32> { };
		struct Inttypel : Register<0x1c, 32> { };

		/**
		 * Normal interrupt priority registers
		 */
		struct Nipriority : Register_array<0x20, 32, 8, 32>
		{
			enum { ALL_LOWEST = 0 };
		};

		/**
		 * Interrupt source registers
		 */
		struct Intsrch : Register<0x48, 32> { };
		struct Intsrcl : Register<0x4c, 32> { };

		/**
		 * Normal interrupt pending registers
		 */
		struct Nipndh : Register<0x58, 32> { };
		struct Nipndl : Register<0x5c, 32> { };

		/**
		 * Normal interrupt vector and status register
		 */
		struct Nivecsr : Register<0x40, 32>
		{
			struct Nvector : Bitfield<16, 16> { };
		};

		public:

			enum { MAX_INTERRUPT_ID = 63 };

			/**
			 * Constructor, enables all interrupts
			 */
			Pic() : Mmio(Board::AVIC_MMIO_BASE)
			{
				mask();
				write<Nimask>(Nimask::NONE_MASKED);
				write<Intcntl>(Intcntl::init_value());
				write<Inttypeh>(Inttype::ALL_IRQS);
				write<Inttypel>(Inttype::ALL_IRQS);
				for (unsigned i = 0; i < Nipriority::ITEMS; i++)
					write<Nipriority>(Nipriority::ALL_LOWEST, i);
			}

			/**
			 * Receive a pending request number 'i'
			 */
			bool take_request(unsigned & i)
			{
				i = read<Nivecsr::Nvector>();
				return valid(i) ? true : false;
			}

			/**
			 * Finish the last taken request
			 */
			void finish_request() {
				/* requests disappear by source retraction or masking */ }

			/**
			 * Validate request number 'i'
			 */
			bool valid(unsigned const i) const {
				return i <= MAX_INTERRUPT_ID; }

			/**
			 * Unmask all interrupts
			 */
			void unmask()
			{
				write<Intenablel>(~0);
				write<Intenableh>(~0);
			}

			/**
			 * Mask all interrupts
			 */
			void mask()
			{
				write<Intenablel>(0);
				write<Intenableh>(0);
			}

			/**
			 * Unmask interrupt 'i'
			 */
			void unmask(unsigned const i) {
				if (i <= MAX_INTERRUPT_ID) write<Intennum>(i); }

			/**
			 * Mask interrupt 'i'
			 */
			void mask(unsigned const i) {
				if (i <= MAX_INTERRUPT_ID) write<Intdisnum>(i); }
	};
}

#endif /* _INCLUDE__IMX31__PIC_H_ */

