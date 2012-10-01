/*
 * \brief  Driver base for the private timer of the ARM Cortex-A9
 * \author Martin stein
 * \date   2011-12-13
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__CPU__CORTEX_A9__TIMER_H_
#define _INCLUDE__DRIVERS__CPU__CORTEX_A9__TIMER_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode
{
	/**
	 * Driver base for the private timer of the ARM Cortex-A9
	 */
	template <unsigned long CLK>
	struct Cortex_a9_timer : public Mmio
	{
		enum { TICS_PER_MS = CLK / 1000, };

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
			struct Timer_enable : Bitfield<0,1> { }; /* 1: 'Counter' decrements, 0: 'Counter' stays 0 */
			struct Auto_reload  : Bitfield<1,1> { }; /* 1: Auto reload mode, 0: One shot mode */
			struct Irq_enable   : Bitfield<2,1> { }; /* 1: IRQ = 'Interrupt_status::Event' 0: IRQ = 0 */
			struct Prescaler    : Bitfield<8,8> { }; /* modifies the clock period for the decrementing */
		};

		/**
		 * Timer interrupt status register
		 */
		struct Interrupt_status : Register<0xc, 32>
		{
			struct Event : Bitfield<0,1> { }; /* 'Event' = !'Counter' */
		};

		/**
		 * Constructor, clears the interrupt output
		 */
		Cortex_a9_timer(addr_t const mmio_base) : Mmio(mmio_base) {
			clear_interrupt(); }

		/**
		 * Start a one-shot run
		 * \param  tics  native timer value used to assess the delay
		 *               of the timer interrupt as of the call
		 */
		inline void start_one_shot(uint32_t const tics);

		/**
		 * Translate milliseconds to a native timer value
		 */
		static uint32_t ms_to_tics(unsigned long const ms) {
			return ms * TICS_PER_MS; }

		/**
		 * Stop the timer and return last timer value
		 */
		unsigned long stop()
		{
			unsigned long const v = read<Counter>();
			write<typename Control::Timer_enable>(0);
			return v;
		}

		/**
		 * Clear interrupt output line
		 */
		void clear_interrupt() { write<typename Interrupt_status::Event>(1); }
	};
}


template <unsigned long CLOCK>
void Genode::Cortex_a9_timer<CLOCK>::start_one_shot(uint32_t const tics)
{
	/* reset timer */
	clear_interrupt();
	write<Control>(Control::Timer_enable::bits(0) |
	               Control::Auto_reload::bits(0) |
	               Control::Irq_enable::bits(1) |
	               Control::Prescaler::bits(0));

	/* load timer and start decrementing */
	write<Load>(tics);
	write<typename Control::Timer_enable>(1);
}


#endif /* _INCLUDE__DRIVERS__CPU__CORTEX_A9__TIMER_H_ */
