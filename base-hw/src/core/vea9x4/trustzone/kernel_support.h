/*
 * \brief  Kernel support specific for the Versatile VEA9X4
 * \author Stefan Kalkowski
 * \date   2012-10-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__VEA9X4__TRUSTZONE__KERNEL_SUPPORT_H_
#define _SRC__CORE__VEA9X4__TRUSTZONE__KERNEL_SUPPORT_H_

/* Core includes */
#include <cortex_a9/cpu/core.h>
#include <pic/pl390_base.h>

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
			{
				/* configure every shared peripheral interrupt */
				for (unsigned i=MIN_SPI; i <= _max_interrupt; i++) {
					_distr.write<Distr::Icdicr::Edge_triggered>(0, i);
					_distr.write<Distr::Icdipr::Priority>(0, i);
					_distr.write<Distr::Icdiptr::Cpu_targets>(Distr::Icdiptr::Cpu_targets::ALL, i);
				}

				/* disable the priority filter */
				_cpu.write<Cpu::Iccpmr::Priority>(0xff);

				/* signal secure IRQ via FIQ interface */
				_cpu.write<Cpu::Iccicr>(Cpu::Iccicr::Enable_s::bits(1)  |
				                        Cpu::Iccicr::Enable_ns::bits(1) |
				                        Cpu::Iccicr::Fiq_en::bits(1));

				/* use whole band of prios */
				_cpu.write<Cpu::Iccbpr::Binary_point>(Cpu::Iccbpr::Binary_point::NO_PREEMPTION);

				/* enable device */
				_distr.write<Distr::Icddcr>(Distr::Icddcr::Enable::bits(1));
			}

			/**
			 * Mark interrupt i unsecure
			 */
			void unsecure(unsigned i) {
				_distr.write<Distr::Icdisr::Nonsecure>(1, i); }
	};

	/**
	 * Kernel timer
	 */
	class Timer : public Cortex_a9::Private_timer { };
}


#endif /* _SRC__CORE__VEA9X4__TRUSTZONE__KERNEL_SUPPORT_H_ */

