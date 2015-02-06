/*
 * \brief  Programmable interrupt controller for core
 * \author Norman Feske
 * \date   2013-04-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
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


class Genode::Pic
{
	public:

		enum {
			/*
			 * FIXME: dummy ipi value on non-SMP platform, should be removed
			 *        when SMP is an aspect of CPUs only compiled where necessary
			 */
			IPI       = 255,
			NR_OF_IRQ = 256,
		};

		/**
		 * Constructor
		 */
		Pic() { }

		void init_cpu_local() { }

		bool take_request(unsigned &irq) { return false; }

		void finish_request() { }

		void mask() { }

		void unmask(unsigned const i, unsigned) { }

		void mask(unsigned const i) { }

		/*
		 * Dummies
		 */

		bool is_ip_interrupt(unsigned, unsigned) { return false; }
		void trigger_ip_interrupt(unsigned) { }
};

namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _PIC_H_ */
