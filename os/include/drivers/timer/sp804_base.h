/*
 * \brief  Basic driver for the ARM SP804 timer
 * \author Martin stein
 * \date   2012-04-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__TIMER__SP804_H_
#define _INCLUDE__DRIVERS__TIMER__SP804_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode
{
	/**
	 * Basic driver for the ARM SP804 timer
	 */
	template <unsigned long CLK>
	class Sp804_base : public Mmio
	{
		enum {
			TICS_PER_MS = CLK / 1000,
			TICS_PER_US = TICS_PER_MS / 1000,

			AVOID_INVALID_TEMPLATE_ARGS = 1 / TICS_PER_US,
		};

		/**
		 * Holds value that shall be loaded to the timer value register
		 */
		struct Load : Register<0x0, 32> { };

		/**
		 * Raw interrupt status
		 */
		struct Ris : Register<0x10, 1> { };

		/**
		 * Background load register
		 */
		struct Bgload : Register<0x18, 32> { };

		/**
		 * Timer value register
		 */
		struct Value : Register<0x4, 32> { enum { MAX_VALUE = 0xffffffff }; };

		/**
		 * Timer control register
		 */
		struct Control : Register<0x8, 8>
		{
			struct Oneshot  : Bitfield<0,1> { };
			struct Size     : Bitfield<1,1> { };
			struct Pre      : Bitfield<2,2> { };
			struct Int_en   : Bitfield<5,1> { };
			struct Mode     : Bitfield<6,1> { };
			struct Timer_en : Bitfield<7,1> { };
		};

		/**
		 * Clears the timer interrupt
		 */
		struct Int_clr : Register<0xc, 1> { };

		public:

			/**
			 * Constructor, clears interrupt output
			 */
			Sp804_base(addr_t const mmio_base) : Mmio(mmio_base) {
				clear_interrupt(); }

			/**
			 * Run the timer in order that it raises IRQ when
			 * it reaches zero, then stop
			 *
			 * \param  tics  native timer value used to assess the delay
			 *               of the timer interrupt as of this call
			 */
			void run_and_stop(unsigned long const tics)
			{
				/* disable and configure timer for a one-shot */
				clear_interrupt();
				write<typename Control::Timer_en>(0);
				write<Control>(Control::Timer_en::bits(0) |
				               Control::Mode::bits(1) |
				               Control::Int_en::bits(1) |
				               Control::Pre::bits(0) |
				               Control::Size::bits(1) |
				               Control::Oneshot::bits(1));

				/* load value and enable timer */
				write<Load>(tics);
				write<typename Control::Timer_en>(1);
			}

			/**
			 * Run the timer in order that it raises IRQ when it reaches zero,
			 * then wrap and continue
			 *
			 * \param  tics  native timer value used to assess the delay
			 *               of the timer interrupt as of this call
			 */
			void run_and_wrap(unsigned long const tics)
			{
				/* configure the timer in order that it reloads on 0 */
				clear_interrupt();
				write<typename Control::Timer_en>(0);
				write<Control>(Control::Timer_en::bits(0) |
				               Control::Mode::bits(1) |
				               Control::Int_en::bits(1) |
				               Control::Pre::bits(0) |
				               Control::Size::bits(1) |
				               Control::Oneshot::bits(0));

				/* start timer with the initial value */
				write<Load>(tics);
				write<typename Control::Timer_en>(1);

				/*
				 * Ensure that the timer loads its max value instead of the
				 * initial value when it reaches 0 in order that it looks like
				 * it wraps.
				 */
				write<Bgload>(max_value());
			}

			/**
			 * Current timer value
			 */
			unsigned long value() const { return read<Value>(); }

			/**
			 * Get timer value and corresponding wrapped status of timer
			 */
			unsigned long value(bool & wrapped) const
			{
				typename Value::access_t v = read<Value>();
				wrapped = (bool)read<Ris>();
				if (!wrapped) return v;
				return read<Value>();
			}

			/**
			 * Clear interrupt output line
			 */
			void clear_interrupt() { write<Int_clr>(1); }

			/**
			 * Translate milliseconds to a native timer value
			 */
			static unsigned long ms_to_tics(unsigned long const ms) {
				return ms * TICS_PER_MS; }

			/**
			 * Translate native timer value to microseconds
			 */
			static unsigned long tics_to_us(unsigned long const tics) {
				return tics / TICS_PER_US; }

			/**
			 * Translate microseconds to a native timer value
			 */
			static unsigned long us_to_tics(unsigned long const us) {
				return us * TICS_PER_US; }

			/**
			 * Translate native timer value to microseconds
			 */
			static unsigned long max_value() { return Value::MAX_VALUE; }
	};
}

#endif /* _INCLUDE__DRIVERS__TIMER__SP804_H_ */

