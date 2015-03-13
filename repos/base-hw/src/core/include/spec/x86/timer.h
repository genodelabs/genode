/*
 * \brief  Timer driver for core
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TIMER_H_
#define _TIMER_H_

#include <util/mmio.h>
#include <base/stdint.h>
#include <base/printf.h>
#include <port_io.h>
#include <board.h>

namespace Genode
{
	/**
	 * LAPIC-based timer driver for core
	 */
	class Timer;
}

class Genode::Timer : public Mmio
{
	private:

		enum {
			/* PIT constants */
			PIT_TICK_RATE  = 1193182ul,
			PIT_SLEEP_MS   = 50,
			PIT_SLEEP_TICS = (PIT_TICK_RATE / 1000) * PIT_SLEEP_MS,
			PIT_CH0_DATA   = 0x40,
			PIT_CH2_DATA   = 0x42,
			PIT_CH2_GATE   = 0x61,
			PIT_MODE       = 0x43,
		};

		/* Timer registers */
		struct Tmr_lvt : Register<0x320, 32>
		{
			struct Vector     : Bitfield<0,  8> { };
			struct Delivery   : Bitfield<8,  3> { };
			struct Mask       : Bitfield<16, 1> { };
			struct Timer_mode : Bitfield<17, 2> { };
		};
		struct Tmr_initial : Register <0x380, 32> { };
		struct Tmr_current : Register <0x390, 32> { };

		uint32_t _tics_per_ms = 0;

		/* Measure LAPIC timer frequency using PIT channel 2 */
		uint32_t pit_calc_timer_freq(void)
		{
			uint32_t t_start, t_end;

			/* Set channel gate high and disable speaker */
			outb(PIT_CH2_GATE, (inb(0x61) & ~0x02) | 0x01);

			/* Set timer counter (mode 0, binary count) */
			outb(PIT_MODE, 0xb0);
			outb(PIT_CH2_DATA, PIT_SLEEP_TICS & 0xff);
			outb(PIT_CH2_DATA, PIT_SLEEP_TICS >> 8);

			write<Tmr_initial>(~0U);

			t_start = read<Tmr_current>();
			while ((inb(PIT_CH2_GATE) & 0x20) == 0)
			{
				asm volatile("pause" : : : "memory");
			}
			t_end = read<Tmr_current>();

			write<Tmr_initial>(0);

			return (t_start - t_end) / PIT_SLEEP_MS;
		}

	public:

		Timer() : Mmio(Board::MMIO_LAPIC_BASE)
		{
			/* Enable LAPIC timer in one-shot mode */
			write<Tmr_lvt::Vector>(Board::TIMER_VECTOR_KERNEL);
			write<Tmr_lvt::Delivery>(0);
			write<Tmr_lvt::Mask>(0);
			write<Tmr_lvt::Timer_mode>(0);

			/* Calculate timer frequency */
			_tics_per_ms = pit_calc_timer_freq();
			PINF("LAPIC: timer frequency %u kHz", _tics_per_ms);
		}

		/**
		 * Disable PIT timer channel. This is necessary since BIOS sets up
		 * channel 0 to fire periodically.
		 */
		static void disable_pit(void)
		{
			outb(PIT_MODE, 0x30);
			outb(PIT_CH0_DATA, 0);
			outb(PIT_CH0_DATA, 0);
		}

		static unsigned interrupt_id(int)
		{
			return Board::TIMER_VECTOR_KERNEL;
		}

		inline void start_one_shot(uint32_t const tics, unsigned)
		{
			write<Tmr_initial>(tics);
		}

		uint32_t ms_to_tics(unsigned const ms)
		{
			return ms * _tics_per_ms;
		}

		unsigned value(unsigned)
		{
			return read<Tmr_current>();
		}
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _TIMER_H_ */
