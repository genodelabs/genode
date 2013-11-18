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

#ifndef _IMX53__PIC_BASE_H_
#define _IMX53__PIC_BASE_H_

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <board.h>

namespace Imx53
{
	using namespace Genode;

	/**
	 * Programmable interrupt controller for core
	 */
	class Pic_base : public Mmio
	{
		public:

			enum { MAX_INTERRUPT_ID = 108 };

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
			struct Intsec : Register_array<0x80, 32, MAX_INTERRUPT_ID, 1>
			{
				struct Nonsecure : Bitfield<0, 1> { };
			};

			/**
			 * Interrupt set enable registers
			 */
			struct Enset : Register_array<0x100, 32, MAX_INTERRUPT_ID, 1, true>
			{
				struct Set_enable : Bitfield<0, 1> { };
			};

			/**
			 * Interrupt clear enable registers
			 */
			struct Enclear : Register_array<0x180, 32, MAX_INTERRUPT_ID, 1, true>
			{
				struct Clear_enable : Bitfield<0, 1> { };
			};

			/**
			 * Interrupt priority level registers
			 */
			struct Priority  : Register_array<0x400, 32, MAX_INTERRUPT_ID, 8>
			{
				enum { MIN_PRIO = 0xff };
			};

			/**
			 * Pending registers
			 */
			struct Pndr  : Register_array<0xd00, 32, MAX_INTERRUPT_ID, 1>
			{
				struct Pending : Bitfield<0, 1> { };
			};

			/**
			 * Highest interrupt pending registers
			 */
			struct Hipndr  : Register_array<0xd80, 32, MAX_INTERRUPT_ID, 1, true>
			{
				struct Pending : Bitfield<0, 1> { };
			};

			/**
			 * Maximum supported interrupt priority
			 */
			unsigned _max_priority() { return 255; }

		public:

			/**
			 * Constructor, all interrupts get masked
			 */
			Pic_base() : Mmio(Board::TZIC_MMIO_BASE)
			{
				for (unsigned i = 0; i <= MAX_INTERRUPT_ID; i++) {
					write<Intsec::Nonsecure>(1, i);
					write<Enclear::Clear_enable>(1, i);
				}

				write<Priomask::Mask>(0x1f);
				write<Intctrl>(Intctrl::Enable::bits(1) |
							   Intctrl::Nsen::bits(1)   |
							   Intctrl::Nsen_mask::bits(1));

			}

			/**
			 * Receive a pending request number 'i'
			 */
			bool take_request(unsigned & i)
			{
				for (unsigned j = 0; j <= MAX_INTERRUPT_ID; j++) {
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
				return i <= MAX_INTERRUPT_ID; }

			/**
			 * Unmask all interrupts
			 */
			void unmask()
			{
				for (unsigned i=0; i <= MAX_INTERRUPT_ID; i++)
					write<Enset::Set_enable>(1, i);
			}

			/**
			 * Mask all interrupts
			 */
			void mask()
			{
				for (unsigned i=0; i <= MAX_INTERRUPT_ID; i++)
					write<Enclear::Clear_enable>(1, i);
			}

			/**
			 * Unmask interrupt 'i'
			 */
			void unmask(unsigned const i)
			{
				if (i <= MAX_INTERRUPT_ID)
					write<Enset::Set_enable>(1, i);
			}

			/**
			 * Mask interrupt 'i'
			 */
			void mask(unsigned const i)
			{
				if (i <= MAX_INTERRUPT_ID)
					write<Enclear::Clear_enable>(1, i);
			}
	};
}

#endif /* _IMX53__PIC_BASE_H_ */
