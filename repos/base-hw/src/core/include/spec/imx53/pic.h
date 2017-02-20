/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2012-10-24
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__IMX53__PIC_H_
#define _CORE__INCLUDE__SPEC__IMX53__PIC_H_

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <board.h>

namespace Genode { class Pic; }


class Genode::Pic : public Mmio
{
	public:

		enum {
			/*
			 * FIXME: dummy ipi value on non-SMP platform, should be removed
			 *        when SMP is an aspect of CPUs only compiled where necessary
			 */
			IPI       = 0,
			NR_OF_IRQ = 109,
		};

	protected:

		/**
		 * Software Interrupt Trigger Register
		 */
		struct Swint : Register<0xf00, 32> {
			struct Intid  : Bitfield<0,10> { }; };

		/**
		 * Interrupt control register
		 */
		struct Intctrl : Register<0, 32>
		{
			struct Enable    : Bitfield<0,1>  { };
			struct Nsen      : Bitfield<16,1> { };
			struct Nsen_mask : Bitfield<31,1> { };
		};

		/**
		 * Priority mask register
		 */
		struct Priomask : Register<0xc, 32> {
			struct Mask : Bitfield<0,8> { }; };

		/**
		 * Interrupt security registers
		 */
		struct Intsec : Register_array<0x80, 32, NR_OF_IRQ, 1> {
			struct Nonsecure : Bitfield<0, 1> { }; };

		/**
		 * Interrupt set enable registers
		 */
		struct Enset : Register_array<0x100, 32, NR_OF_IRQ, 1, true> {
			struct Set_enable : Bitfield<0, 1> { }; };

		/**
		 * Interrupt clear enable registers
		 */
		struct Enclear : Register_array<0x180, 32, NR_OF_IRQ, 1, true> {
			struct Clear_enable : Bitfield<0, 1> { }; };

		/**
		 * Interrupt priority level registers
		 */
		struct Priority  : Register_array<0x400, 32, NR_OF_IRQ, 8> { };

		/**
		 * Highest interrupt pending registers
		 */
		struct Hipndr  : Register_array<0xd80, 32, NR_OF_IRQ, 1, true> {
			struct Pending : Bitfield<0, 1> { }; };

		/**
		 * Initializes security extension if needed
		 */
		void _init_security_ext();

	public:

		Pic();

		void init_cpu_local()
		{
			for (unsigned i = 0; i < NR_OF_IRQ; i++) {
				write<Intsec::Nonsecure>(1, i);
				write<Enclear::Clear_enable>(1, i);
			}
			write<Priomask::Mask>(0x1f);
			Intctrl::access_t v = 0;
			Intctrl::Enable::set(v, 1);
			Intctrl::Nsen::set(v, 1);
			Intctrl::Nsen_mask::set(v, 1);
			write<Intctrl>(v);
			_init_security_ext();
		}

		/**
		 * Mark interrupt 'i' unsecure
		 */
		void unsecure(unsigned const i);

		/**
		 * Mark interrupt 'i' secure
		 */
		void secure(unsigned const i);

		/**
		 * Receive a pending request number 'i'
		 */
		bool take_request(unsigned & i)
		{
			for (unsigned j = 0; j < NR_OF_IRQ; j++) {
				if (!read<Hipndr::Pending>(j)) { continue; }
				i = j;
				return true;
			}
			return false;
		}

		/**
		 * Validate request number 'i'
		 */
		bool valid(unsigned const i) const { return i < NR_OF_IRQ; }

		/**
		 * Unmask interrupt 'i'
		 */
		void unmask(unsigned const i, unsigned) {
			if (valid(i)) { write<Enset::Set_enable>(1, i); } }

		/**
		 * Mask interrupt 'i'
		 */
		void mask(unsigned const i) {
			if (valid(i)) { write<Enclear::Clear_enable>(1, i); } }


		/*
		 * Trigger interrupt 'i' from software if possible
		 */
		void trigger(unsigned const i) {
			write<Swint>(Swint::Intid::bits(i)); }


		/*************
		 ** Dummies **
		 *************/

		void trigger_ip_interrupt(unsigned) { }
		void finish_request() { }
};

namespace Kernel { using Pic = Genode::Pic; }

#endif /* _CORE__INCLUDE__SPEC__IMX53__PIC_H_ */
