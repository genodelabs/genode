/*
 * \brief  Timer driver for core
 * \author Martin stein
 * \date   2011-12-13
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__CORTEX_A9__TIMER_H_
#define _CORE__INCLUDE__SPEC__CORTEX_A9__TIMER_H_

/* base-hw includes */
#include <kernel/types.h>

/* Genode includes */
#include <util/mmio.h>

/* core includes */
#include <board.h>

namespace Genode { class Timer; }

/**
 * Timer driver for core
 */
class Genode::Timer : public Mmio
{
	private:

		using time_t = Kernel::time_t;

		enum {
			TICS_PER_MS =
				Board::CORTEX_A9_PRIVATE_TIMER_CLK /
				Board::CORTEX_A9_PRIVATE_TIMER_DIV / 1000
		};

		/**
		 * Load value register
		 */
		struct Load : Register<0x0, 32> { };

		/**
		 * Counter value register
		 */
		struct Counter : Register<0x4, 32> { };

		/**
		 * Timer control register
		 */
		struct Control : Register<0x8, 32>
		{
			struct Timer_enable : Bitfield<0,1> { }; /* enable counting */
			struct Irq_enable   : Bitfield<2,1> { }; /* unmask interrupt */
			struct Prescaler    : Bitfield<8,8> { };
		};

		/**
		 * Timer interrupt status register
		 */
		struct Interrupt_status : Register<0xc, 32>
		{
			struct Event : Bitfield<0,1> { }; /* if counter hit zero */
		};

	public:

		Timer();

		/**
		 * Return kernel name of timer interrupt
		 */
		static unsigned interrupt_id(unsigned const) {
			return Board::PRIVATE_TIMER_IRQ; }

		/**
		 * Start single timeout run
		 *
		 * \param tics  delay of timer interrupt
		 */
		void start_one_shot(time_t const tics, unsigned const);

		time_t tics_to_us(time_t const tics) const
		{
			/*
			 * If we would do the translation with one division and
			 * multiplication over the whole argument, we would loose
			 * microseconds granularity although the timer frequency would
			 * allow for such granularity. Thus, we treat the most significant
			 * half and the least significant half of the argument separate.
			 * Each half is shifted to the best bit position for the
			 * translation, then translated, and then shifted back.
			 */
			static_assert(TICS_PER_MS >= 1000, "Bad TICS_PER_MS value");
			enum {
				HALF_WIDTH = (sizeof(time_t) << 2),
				MSB_MASK  = ~0UL << HALF_WIDTH,
				LSB_MASK  = ~0UL >> HALF_WIDTH,
				MSB_RSHIFT = 10,
				LSB_LSHIFT = HALF_WIDTH - MSB_RSHIFT,
			};
			time_t const msb = (((tics >> MSB_RSHIFT)
			                     * 1000) / TICS_PER_MS) << MSB_RSHIFT;
			time_t const lsb = ((((tics & LSB_MASK) << LSB_LSHIFT)
			                     * 1000) / TICS_PER_MS) >> LSB_LSHIFT;
			return (msb & MSB_MASK) | lsb;
		}

		time_t us_to_tics(time_t const us) const {
			return (us / 1000) * TICS_PER_MS; }

		/**
		 * Return current native timer value
		 */
		time_t value(unsigned const) { return read<Counter>(); }

		time_t max_value() { return (Load::access_t)~0; }
};

namespace Kernel { using Genode::Timer; }

#endif /* _CORE__INCLUDE__SPEC__CORTEX_A9__TIMER_H_ */
