/*
 * \brief  Timer for core
 * \author Martin stein
 * \date   2011-12-13
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CORTEX_A9__TIMER_H_
#define _INCLUDE__CORTEX_A9__TIMER_H_

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <cortex_a9/cpu.h>

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
		 * Timer counter value register
		 */
		struct Counter : Register<0x4, 32> { };

		/**
		 * Timer control register
		 */
		struct Control : Register<0x8, 32>
		{
			struct Timer_enable : Bitfield<0,1> { }; /* enable counting */
			struct Auto_reload  : Bitfield<1,1> { }; /* reload at zero */
			struct Irq_enable   : Bitfield<2,1> { }; /* unmask interrupt */
			struct Prescaler    : Bitfield<8,8> { }; /* modify frequency */

			/**
			 * Configure for a one-shot
			 */
			static access_t init_one_shot()
			{
				return Timer_enable::bits(0) |
				       Auto_reload::bits(0) |
				       Irq_enable::bits(1) |
				       Prescaler::bits(0);
			}
		};

		/**
		 * Timer interrupt status register
		 */
		struct Interrupt_status : Register<0xc, 32>
		{
			struct Event : Bitfield<0,1> { }; /* if counter hit zero */
		};

		/**
		 * Stop counting
		 */
		void _disable() { write<Control::Timer_enable>(0); }

		public:

			enum { IRQ = Cortex_a9::Cpu::PRIVATE_TIMER_IRQ };

			/**
			 * Constructor, clears the interrupt output
			 */
			Timer() : Mmio(Cortex_a9::Cpu::PRIVATE_TIMER_MMIO_BASE)
			{
				_disable();
				clear_interrupt();
			}

			/**
			 * Start a one-shot run
			 * \param  tics  native timer value used to assess the delay
			 *               of the timer interrupt as of the call
			 */
			inline void start_one_shot(uint32_t const tics)
			{
				/* reset timer */
				clear_interrupt();
				write<Control>(Control::init_one_shot());

				/* load timer and start decrementing */
				write<Load>(tics);
				write<Control::Timer_enable>(1);
			}

			/**
			 * Translate milliseconds to a native timer value
			 */
			static uint32_t ms_to_tics(unsigned long const ms) {
				return ms * TICS_PER_MS; }

			/**
			 * Stop the timer and return last timer value
			 */
			unsigned long stop_one_shot()
			{
				unsigned long const v = read<Counter>();
				_disable();
				return v;
			}

			/**
			 * Clear interrupt output line
			 */
			void clear_interrupt() {
				write<Interrupt_status::Event>(1); }
	};
}

#endif /* _INCLUDE__CORTEX_A9__TIMER_H_ */

