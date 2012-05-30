/*
 * \brief  Parts of kernel support that are identical for all Cortex A9 systems
 * \author Martin Stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CORTEX_A9__KERNEL_SUPPORT_H_
#define _CORE__INCLUDE__CORTEX_A9__KERNEL_SUPPORT_H_

/* Genode includes */
#include <drivers/cpu/cortex_a9/core.h>
#include <drivers/pic/pl390_base.h>

/**
 * CPU driver
 */
class Cpu : public Genode::Cortex_a9 { };

namespace Kernel
{
	/* import Genode types */
	typedef Genode::Cortex_a9 Cortex_a9;
	typedef Genode::Pl390_base Pl390_base;

	/**
	 * Kernel interrupt-controller
	 */
	class Pic : public Pl390_base
	{
		public:

			/**
			 * Constructor
			 */
			Pic() : Pl390_base(Cortex_a9::PL390_DISTRIBUTOR_MMIO_BASE,
			                   Cortex_a9::PL390_CPU_MMIO_BASE)
			{ }
	};

	/**
	 * Kernel timer
	 */
	class Timer : public Cortex_a9::Private_timer { };
}

#endif /* _CORE__INCLUDE__CORTEX_A9__KERNEL_SUPPORT_H_ */

