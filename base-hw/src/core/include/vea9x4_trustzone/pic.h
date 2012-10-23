/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2012-10-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VEA9X4_TRUSTZONE__PIC_H_
#define _INCLUDE__VEA9X4_TRUSTZONE__PIC_H_

/* core includes */
#include <cortex_a9/cpu.h>
#include <cortex_a9/pic.h>

namespace Vea9x4_trustzone
{
	using namespace Genode;

	/**
	 * Programmable interrupt controller for core
	 */
	class Pic : public Cortex_a9::Pic
	{
		public:

			/**
			 * Constructor
			 */
			Pic()
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
			 * Mark interrupt 'i' unsecure
			 */
			void unsecure(unsigned const i) {
				_distr.write<Distr::Icdisr::Nonsecure>(1, i); }
	};
}

#endif /* _INCLUDE__VEA9X4_TRUSTZONE__PIC_H_ */

