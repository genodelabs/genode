/*
 * \brief  Programmable interrupt controller for core
 * \author Martin stein
 * \date   2011-10-26
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PIC_H_
#define _PIC_H_

/* core includes */
#include <spec/arm_gic/pic_support.h>
#include <cpu.h>

namespace Genode
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Pic;
}

class Genode::Pic : public Arm_gic
{
	public:

		/**
		 * Constructor
		 */
		Pic() : Arm_gic(Cpu::PL390_DISTRIBUTOR_MMIO_BASE,
		                Cpu::PL390_CPU_MMIO_BASE) { }

		/**
		 * Mark interrupt 'i' unsecure
		 */
		void unsecure(unsigned const i);
};

namespace Kernel { class Pic : public Genode::Pic { }; }

#endif /* _PIC_H_ */
