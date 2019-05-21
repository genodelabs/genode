/*
 * \brief  Programmable interrupt controller for core
 * \author Stefan Kalkowski
 * \date   2019-05-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__RPI3__PIC_H_
#define _CORE__SPEC__RPI3__PIC_H_

#include <util/mmio.h>
#include <board.h>

namespace Genode { class Pic; }
namespace Kernel { using Pic = Genode::Pic; }

class Genode::Pic : Mmio
{
	public:

		enum {
			TIMER_IRQ = 1,
			NR_OF_IRQ = 64,

			/*
			 * dummy IPI value on non-SMP platform,
			 * only used in interrupt reservation within generic code
			 */
			IPI,
		};

	private:


		struct Core0_timer_irq_control : Register<0x40, 32>
		{
			struct Cnt_p_ns_irq : Bitfield<1, 1> {};
		};

		struct Core1_timer_irq_control : Register<0x44, 32> {};
		struct Core2_timer_irq_control : Register<0x48, 32> {};
		struct Core3_timer_irq_control : Register<0x4c, 32> {};

		struct Core0_irq_source : Register<0x60, 32> {};
		struct Core1_irq_source : Register<0x64, 32> {};
		struct Core2_irq_source : Register<0x68, 32> {};
		struct Core3_irq_source : Register<0x6c, 32> {};

	public:

		Pic();

		void init_cpu_local();
		bool take_request(unsigned &irq);
		void finish_request() { }
		void mask();
		void unmask(unsigned const i, unsigned);
		void mask(unsigned const i);

		static constexpr bool fast_interrupts() { return false; }
};

#endif /* _CORE__SPEC__RPI3__PIC_H_ */
