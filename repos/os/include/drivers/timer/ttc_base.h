/*
 * \brief  Basic driver for the Zynq Triple Counter Timer
 * \author Johannes Schlatow
 * \date   2015-03-05
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__TIMER__TTC_BASE_H_
#define _INCLUDE__DRIVERS__TIMER__TTC_BASE_H_

/* Genode includes */
#include <util/mmio.h>

namespace Genode
{
	/**
	 * Basic driver for the Zynq TTC
	 *
	 * The TTC has three timers. The index of the timer to be used must be provides
	 * as a template argument (0-2).
	 *
	 * (see Xilinx ug585)
	 */
	template <unsigned IDX, unsigned long CLK>
	class Ttc_base : public Mmio
	{
		enum {
			TICS_PER_MS = CLK / 1000,
			TICS_PER_US = TICS_PER_MS / 1000,
		};

		public:

			/**
			 * Constructor, clears interrupt output
			 */
			Ttc_base(addr_t const mmio_base) : Mmio(mmio_base + IDX*0x04)
			{
				clear_interrupt();
				write<typename Control::Disable>(1);

				/* enable all interrupts */
				write<Irqen>(0xff);

				/* set match registers to 0 */
				write<Match1>(0);
				write<Match2>(0);
				write<Match3>(0);

				/* set maximum interval so that max_value() does not return 0 */
				write<Interval>(Value::max_value());
			}

			/* Clock control register */
			struct Clock : Register<0x00, 8>
			{
				struct Prescale_en  : Bitfield<0, 1> { };
				struct Prescale     : Bitfield<1, 4> { };
				struct Clk_src      : Bitfield<5, 1>
				{
					enum {
						PCLK = 0,
						EXT = 1
					};
				};
				struct Ext_edge     : Bitfield<6, 1> { };
			};

			/* Counter control register */
			struct Control : Register<0x0C, 8>
			{
				struct Disable    : Bitfield<0,1> { };
				struct Mode       : Bitfield<1,1> {
					enum {
						OVERFLOW = 0,
						INTERVAL = 1
					};
				};
				struct Decrement  : Bitfield<2, 1> { };
				struct Match      : Bitfield<3, 1> { };
				struct Reset      : Bitfield<4, 1> { };
				struct Wave_en    : Bitfield<5, 1> { };
				struct Wave_pol   : Bitfield<6, 1> { };
			};

			/* Counter value */
			struct Value : Register<0x18, 16>
			{
				static access_t max_value() { return ~0; }
			};

			struct Interval : Register<0x24, 16> { };

			struct Match1 : Register<0x30, 16> { };

			struct Match2 : Register<0x3C, 16> { };

			struct Match3 : Register<0x48, 16> { };

			struct Irq : Register<0x54, 8>
			{
				struct Interval : Bitfield<0, 1> { };
				struct Match1   : Bitfield<1, 1> { };
				struct Match2   : Bitfield<2, 1> { };
				struct Match3   : Bitfield<3, 1> { };
				struct Overflow : Bitfield<4, 1> { };
				struct EventTmr : Bitfield<5, 1> { };
			};

			struct Irqen : Register<0x60, 8>
			{
				struct Interval : Bitfield<0, 1> { };
				struct Match1   : Bitfield<1, 1> { };
				struct Match2   : Bitfield<2, 1> { };
				struct Match3   : Bitfield<3, 1> { };
				struct Overflow : Bitfield<4, 1> { };
				struct EventTmr : Bitfield<5, 1> { };
			};

			struct EventTmrCtrl : Register<0x6C, 8>
			{
				struct Enable   : Bitfield<0, 1> { };
				struct Low      : Bitfield<1, 1> { };
				struct Overflow : Bitfield<2, 1> {
					enum {
						ONE_SHOT = 0,
						CONTINUE = 1
					};
				};
			};

			struct EventTmr : Register<0x78, 16> { };

			/**
			 * Run the timer in order that it raises IRQ when
			 * it reaches zero, then stop
			 *
			 * FIXME not sure how this can be implemented
			 *
			 * \param  tics  native timer value used to assess the delay
			 *               of the timer interrupt as of this call
			 */
			void run_and_stop(unsigned long const tics)
			{
				/* configure timer for a one-shot */
				clear_interrupt();
				write<Control>(Control::Disable::bits(1) |
				            Control::Mode::bits(Control::Mode::INTERVAL) |
				            Control::Decrement::bits(1) |
				            Control::Match::bits(0) | 
				            Control::Reset::bits(0) | 
								Control::Wave_en::bits(1));

				/* load and enable timer */
				write<Interval>(tics);
				write<typename Control::Disable>(0);
			}

			/**
			 * Run the timer in order that it raises IRQ when it reaches the target value,
			 * then continue
			 *
			 * \param  tics  native timer value used to assess the delay
			 *               of the timer interrupt as of this call
			 */
			void run_and_wrap(unsigned long const tics)
			{
				/* configure the timer in order that it reloads on timeout */
				clear_interrupt();
				write<Control>(Control::Disable::bits(1) |
				            Control::Mode::bits(Control::Mode::INTERVAL) |
				            Control::Decrement::bits(1) |
				            Control::Match::bits(0) |
				            Control::Reset::bits(0) |
								Control::Wave_en::bits(1));

				/* Set initial timer value and enable timer */
				write<Interval>(tics);
				write<typename Control::Disable>(0);
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
				unsigned long v = read<Value>();
				wrapped = (bool)read<Irq>();
				return wrapped ? read<Value>() : v;
			}

			/**
			 * Clear interrupt output line
			 */
			void clear_interrupt() { read<Irq>(); }

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
			unsigned long max_value() const { return read<Interval>(); }
	};
}

#endif /* _INCLUDE__DRIVERS__TIMER__A9_PRIVATE_TIMER_H_ */

