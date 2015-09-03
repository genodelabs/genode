/*
 * \brief  Platform-timer base specific for base-hw and zynq
 * \author Johannes Schlatow
 * \date   2015-03-05
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _HW__ZYNQ__PLATFORM_TIMER_BASE_H_
#define _HW__ZYNQ__PLATFORM_TIMER_BASE_H_

/* Genode includes */
#include <io_mem_session/connection.h>
#include <util/mmio.h>
#include <drivers/board_base.h>

/**
 * Platform-timer base specific for base-hw and zynq
 *
 * Uses the internal timer 0 of the TTC. For more details, see Xilinx ug585
 * "Zynq 7000 Technical Reference Manual".
 */
class Platform_timer_base
:
	public Genode::Io_mem_connection,
	public Genode::Mmio
{
	private:

		enum
		{
			PRESCALER   = 5,
			TICS_PER_MS = (Genode::Board_base::CPU_1X_CLOCK / 1000)
			              >> PRESCALER
		};

		/**
		 * Clock control register
		 */
		struct Clock : Register<0x00, 8>
		{
			struct Prescale_en : Bitfield<0, 1> { };
			struct Prescale    : Bitfield<1, 4> { };
		};

		/**
		 * Counter control register
		 */
		struct Control : Register<0x0c, 8>
		{
			struct Disable    : Bitfield<0, 1> { };
			struct Mode       : Bitfield<1, 1> { enum { INTERVAL = 1 }; };
			struct Decrement  : Bitfield<2, 1> { };
			struct Wave_en    : Bitfield<5, 1> { };
		};

		/**
		 * Counter value
		 */
		struct Value    : Register<0x18, 16> { };
		struct Interval : Register<0x24, 16> { };
		struct Match1   : Register<0x30, 16> { };
		struct Match2   : Register<0x3c, 16> { };
		struct Match3   : Register<0x48, 16> { };
		struct Irq      : Register<0x54,  8> { };
		struct Irqen    : Register<0x60,  8> { };

		void _disable()
		{
			write<Control::Disable>(0);
			read<Irq>();
		}

	public:

		enum { IRQ = Genode::Board_base::TTC0_IRQ_0 };

		/**
		 * Constructor
		 */
		Platform_timer_base()
		:
			Io_mem_connection(Genode::Board_base::TTC0_MMIO_BASE,
			                  Genode::Board_base::TTC0_MMIO_SIZE),
			Mmio((Genode::addr_t)
				Genode::env()->rm_session()->attach(dataspace()))
		{
			_disable();

			/* configure prescaler */
			Clock::access_t clock = read<Clock>();
			Clock::Prescale::set(clock, PRESCALER - 1);
			Clock::Prescale_en::set(clock, 1);
			write<Clock>(clock);

			/* enable all interrupts */
			write<Irqen>(~0);

			/* set match registers to 0 */
			write<Match1>(0);
			write<Match2>(0);
			write<Match3>(0);
		}

		/**
		 * Count down 'value', raise IRQ output, wrap counter and continue
		 */
		void run_and_wrap(unsigned long const tics)
		{
			_disable();

			/* configure timer for a one-shot */
			Control::access_t control = 0;
			Control::Mode::set(control, Control::Mode::INTERVAL);
			Control::Decrement::set(control, 1);
			Control::Wave_en::set(control, 1);
			write<Control>(control);

			/* load and enable timer */
			write<Interval>(tics);
			write<Control::Disable>(0);
		}

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
		 * Translate native timer value to microseconds
		 */
		static unsigned long tics_to_us(unsigned long const tics) {
			return tics * 1000 / TICS_PER_MS; }

		/**
		 * Translate microseconds to a native timer value
		 */
		static unsigned long us_to_tics(unsigned long const us) {
			return us * TICS_PER_MS / 1000; }

		/**
		 * Return maximum number of tics
		 */
		unsigned long max_value() const { return (Interval::access_t)~0; }
};

#endif /* _HW__ZYNQ__PLATFORM_TIMER_BASE_H_ */

