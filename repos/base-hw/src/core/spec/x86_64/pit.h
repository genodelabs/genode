/*
 * \brief  Timer driver for core
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__CORE__SPEC__ARM__PIT_H_
#define _SRC__CORE__SPEC__ARM__PIT_H_

/* Genode includes */
#include <util/mmio.h>
#include <base/stdint.h>

/* core includes */
#include <port_io.h>

namespace Board { class Timer; }


/**
 * LAPIC-based timer driver for core
 */
struct Board::Timer: Genode::Mmio<Hw::Cpu_memory_map::LAPIC_SIZE>
{
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
			Genode::Bitset_2<Divide_value_0_2, Divide_value_2_1>
		{
			enum { MAX = 6 };
		};
	};

	struct Calibration_failed : Genode::Exception { };

	Divide_configuration::access_t divider      = 0;
	Genode::uint32_t               ticks_per_ms = 0;

	/* Measure LAPIC timer frequency using PIT channel 2 */
	Genode::uint32_t pit_calc_timer_freq(void);

	Timer(unsigned);

	void init();
};

#endif /* _SRC__CORE__SPEC__ARM__PIT_H_ */
