/*
 * \brief  Programmable interrupt controller for core
 * \author Sebastian Sumpf
 * \date   2015-06-02
 *
 * There currently is no interrupt controller defined for the RISC-V platform.
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PIC_H_
#define _PIC_H_

namespace Genode { class Pic; }

/**
 * Dummy PIC driver for core
 */
class Genode::Pic
{
	public:

		enum {
			/*
			 * FIXME: dummy ipi value on non-SMP platform, should be removed
			 *        when SMP is an aspect of CPUs only compiled where necessary
			 */
			IPI       = 0,
			NR_OF_IRQ = 15,
		};

		Pic() { }
		bool take_request(unsigned & i) { i = 0; return true; }
		void unmask(unsigned const i, unsigned) { }
		void mask(unsigned const i) { }
		void finish_request() { }
};

namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _PIC_H_ */
