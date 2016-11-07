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

#ifndef _CORE__INCLUDE__SPEC__X86_64__TIMER_H_
#define _CORE__INCLUDE__SPEC__X86_64__TIMER_H_

/* base-hw includes */
#include <kernel/types.h>

/* Genode includes */
#include <util/mmio.h>
#include <base/stdint.h>

/* core includes */
#include <port_io.h>
#include <board.h>

namespace Genode { class Timer; }

/**
 * LAPIC-based timer driver for core
 */
class Genode::Timer : public Mmio
{
	private:

		using time_t = Kernel::time_t;

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

		struct Divide_configuration : Register <0x03e0, 32>
		{
			struct Divide_value_0_2 : Bitfield<0, 2> { };
			struct Divide_value_2_1 : Bitfield<3, 1> { };
			struct Divide_value :
				Bitset_2<Divide_value_0_2, Divide_value_2_1>
			{
				enum { MAX = 6 };
			};
		};

		uint32_t _tics_per_ms = 0;

		/* Measure LAPIC timer frequency using PIT channel 2 */
		uint32_t _pit_calc_timer_freq(void)
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
			write<Divide_configuration::Divide_value>(
				Divide_configuration::Divide_value::MAX);

			/* Enable LAPIC timer in one-shot mode */
			write<Tmr_lvt::Vector>(Board::TIMER_VECTOR_KERNEL);
			write<Tmr_lvt::Delivery>(0);
			write<Tmr_lvt::Mask>(0);
			write<Tmr_lvt::Timer_mode>(0);

			/* Calculate timer frequency */
			_tics_per_ms = _pit_calc_timer_freq();
		}

		/**
		 * Disable PIT timer channel. This is necessary since BIOS sets up
		 * channel 0 to fire periodically.
		 */
		static void disable_pit()
		{
			outb(PIT_MODE, 0x30);
			outb(PIT_CH0_DATA, 0);
			outb(PIT_CH0_DATA, 0);
		}

		static unsigned interrupt_id(unsigned const) {
			return Board::TIMER_VECTOR_KERNEL; }

		void start_one_shot(time_t const tics, unsigned const) {
			write<Tmr_initial>(tics); }

		time_t tics_to_us(time_t const tics) const {
			return (tics / _tics_per_ms) * 1000; }

		time_t us_to_tics(time_t const us) const {
			return (us / 1000) * _tics_per_ms; }

		time_t max_value() { return (Tmr_initial::access_t)~0; }

		time_t value(unsigned const) { return read<Tmr_current>(); }
};

namespace Kernel { class Timer : public Genode::Timer { }; }

#endif /* _CORE__INCLUDE__SPEC__X86_64__TIMER_H_ */
