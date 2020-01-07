/*
 * \brief  Programmable interrupt controller for core
 * \author Reto Buerki
 * \date   2015-04-14
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__X86_64__MUEN__PIC_H_
#define _CORE__SPEC__X86_64__MUEN__PIC_H_

namespace Board
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Pic;
}

class Board::Pic
{
	public:

		struct Virtual_context {};

		enum {
			/*
			 * FIXME: dummy ipi value on non-SMP platform, should be removed
			 *        when SMP is an aspect of CPUs only compiled where
			 *        necessary
			 */
			IPI       = 255,
			NR_OF_IRQ = 256,
		};

		void irq_occurred(unsigned irq)
		{
			if (irq < NR_OF_IRQ)
				isr[irq] = true;
		}

		bool take_request(unsigned &irq)
		{
			for (int i = 0; i < NR_OF_IRQ; i++) {
				if (isr[i] == true) {
					irq = i;
					isr[i] = false;
					return true;
				}
			}
			return false;
		}

		/*
		 * Dummies
		 */
		Pic() { }
		void finish_request() { }
		void unmask(unsigned const, unsigned) { }
		void mask(unsigned const) { }
		bool is_ip_interrupt(unsigned, unsigned) { return false; }
		void store_apic_id(unsigned const) { }
		void irq_mode(unsigned, unsigned, unsigned) { }

	private:

		bool isr[NR_OF_IRQ] = {false};
};

#endif /* _CORE__SPEC__X86_64__MUEN__PIC_H_ */
