/*
 * \brief  Programmable interrupt controller for core
 * \author Martin Stein
 * \date   2012-10-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CORTEX_A9__NO_TRUSTZONE__PIC_H_
#define _INCLUDE__CORTEX_A9__NO_TRUSTZONE__PIC_H_

/* core includes */
#include <cortex_a9/cpu.h>
#include <cortex_a9/pic.h>

namespace Cortex_a9_no_trustzone
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
				/* disable device */
				_distr.write<Distr::Icddcr::Enable>(0);
				_cpu.write<Cpu::Iccicr::Enable>(0);
				mask();

				/* supported priority range */
				unsigned const min_prio = _distr.min_priority();
				unsigned const max_prio = _distr.max_priority();

				/* configure every shared peripheral interrupt */
				for (unsigned i=MIN_SPI; i <= _max_interrupt; i++)
				{
					_distr.write<Distr::Icdicr::Edge_triggered>(0, i);
					_distr.write<Distr::Icdipr::Priority>(max_prio, i);
					_distr.write<Distr::Icdiptr::Cpu_targets>(
						Distr::Icdiptr::Cpu_targets::ALL, i);
				}

				/* disable the priority filter */
				_cpu.write<Cpu::Iccpmr::Priority>(min_prio);

				/* disable preemption of interrupt handling by interrupts */
				_cpu.write<Cpu::Iccbpr::Binary_point>(
					Cpu::Iccbpr::Binary_point::NO_PREEMPTION);

				/* enable device */
				_distr.write<Distr::Icddcr::Enable>(1);
				_cpu.write<Cpu::Iccicr::Enable>(1);
			}
	};
}

#endif /* _INCLUDE__CORTEX_A9__NO_TRUSTZONE__PIC_H_ */

