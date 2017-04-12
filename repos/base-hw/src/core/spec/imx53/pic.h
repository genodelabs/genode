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

#ifndef _CORE__SPEC__IMX53__PIC_H_
#define _CORE__SPEC__IMX53__PIC_H_

#include <hw/spec/arm/imx_tzic.h>

namespace Genode { class Pic; }


class Genode::Pic : public Hw::Pic
{
	public:

		enum {
			/*
			 * FIXME: dummy ipi value on non-SMP platform, should be removed
			 *        when SMP is an aspect of CPUs only compiled where necessary
			 */
			IPI = 0,
		};

		/*
		 * Trigger interrupt 'i' from software if possible
		 */
		void trigger(unsigned const i) {
			write<Swint>(Swint::Intid::bits(i)); }

		void trigger_ip_interrupt(unsigned) { }

		bool secure(unsigned i) {
			return !read<Intsec::Nonsecure>(i); }
};

namespace Kernel { using Pic = Genode::Pic; }

#endif /* _CORE__SPEC__IMX53__PIC_H_ */
