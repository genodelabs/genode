/*
 * \brief  Timer for core
 * \author Martin stein
 * \date   2011-12-13
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER__CORTEX_A9_H_
#define _TIMER__CORTEX_A9_H_

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <cpu/cortex_a9.h>

namespace Cortex_a9
{
	/**
	 * Timer for core
	 */
	class Timer : public Mmio
	{
		enum { TICS_PER_MS = Cortex_a9::Cpu::PRIVATE_TIMER_CLK / 1000, };

		/**
		 * Load value register
		 */
		struct Load : Register<0x0, 32> { };

		/**
		 * Timer control register
		 */
		struct Control : Register<0x8, 32>
		{
			struct Timer_enable : Bitfield<0,1> { }; /* enable counting */
			struct Irq_enable   : Bitfield<2,1> { }; /* unmask interrupt */
		};

		/**
		 * Timer interrupt status register
		 */
		struct Interrupt_status : Register<0xc, 32>
		{
			struct Event : Bitfield<0,1> { }; /* if counter hit zero */
		};

		public:

			enum { IRQ = Cortex_a9::Cpu::PRIVATE_TIMER_IRQ };

			/**
			 * Constructor, clears the interrupt output
			 */
			Timer() : Mmio(Cortex_a9::Cpu::PRIVATE_TIMER_MMIO_BASE)
			{
				write<Control::Timer_enable>(0);
				clear_interrupt();
			}

			/**
			 * Start one-shot run with an IRQ delay of 'tics'
			 */
			inline void start_one_shot(uint32_t const tics)
			{
				/* reset timer */
				clear_interrupt();
				Control::access_t control = 0;
				Control::Irq_enable::set(control, 1);
				write<Control>(control);

				/* load timer and start decrementing */
				write<Load>(tics);
				write<Control::Timer_enable>(1);
			}

			/**
			 * Translate 'ms' milliseconds to a native timer value
			 */
			static uint32_t ms_to_tics(unsigned const ms)
			{
				return ms * TICS_PER_MS;
			}

			/**
			 * Clear interrupt output line
			 */
			void clear_interrupt()
			{
				write<Interrupt_status::Event>(1);
			}
	};
}

#endif /* _TIMER__CORTEX_A9_H_ */

