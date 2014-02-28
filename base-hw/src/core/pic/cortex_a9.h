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

#ifndef _PIC__CORTEX_A9_H_
#define _PIC__CORTEX_A9_H_

/* core includes */
#include <pic/arm_gic.h>
#include <processor_driver.h>

namespace Cortex_a9
{
	/**
	 * Programmable interrupt controller for core
	 */
	class Pic : public Arm_gic::Pic
	{
		public:

			/**
			 * Constructor
			 */
			Pic() : Arm_gic::Pic(Genode::Cpu::PL390_DISTRIBUTOR_MMIO_BASE,
			                     Genode::Cpu::PL390_CPU_MMIO_BASE) { }
	};
}

#endif /* _PIC__CORTEX_A9_H_ */

