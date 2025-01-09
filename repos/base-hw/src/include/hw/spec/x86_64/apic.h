/*
 * \brief   APIC definitions
 * \author  Stefan Kalkowski
 * \date    2024-04-25
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__X86_64__APIC_H_
#define _SRC__INCLUDE__HW__SPEC__X86_64__APIC_H_

namespace Hw { class Local_apic; }

#include <hw/spec/x86_64/x86_64.h>

/* Genode includes */
#include <drivers/timer/util.h>

struct Hw::Local_apic : Genode::Mmio<Hw::Cpu_memory_map::LAPIC_SIZE>
{
	struct Id  : Register<0x020, 32> { };
	struct EOI : Register<0x0b0, 32, true> { };
	struct Svr : Register<0x0f0, 32>
	{
		struct APIC_enable : Bitfield<8, 1> { };
	};

	/*
	 * ISR register, see Intel SDM Vol. 3A, section 10.8.4.
	 *
	 * Each of the 8 32-bit ISR values is followed by 12 bytes of padding.
	 */
	struct Isr : Register_array<0x100, 32, 8 * 4, 32> { };

	/*
	 * Interrupt control register
	 */
	struct Icr_low  : Register<0x300, 32, true>
	{
		struct Vector          : Bitfield< 0, 8> { };
		struct Delivery_mode   : Bitfield< 8, 3>
		{
			enum Mode { INIT = 5, SIPI = 6 };
		};

		struct Delivery_status : Bitfield<12, 1> { };
		struct Level_assert    : Bitfield<14, 1> { };
		struct Dest_shorthand  : Bitfield<18, 2>
		{
			enum { ALL_OTHERS = 3 };
		};
	};

	struct Icr_high : Register<0x310, 32, true>
	{
		struct Destination : Bitfield<24, 8> { };
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

	struct Calibration { uint32_t freq_khz; uint32_t div; };

	Calibration calibrate_divider(auto calibration_fn)
	{
		Calibration result { };

		/* calibrate LAPIC frequency to fullfill our requirements */
		for (Divide_configuration::access_t div = Divide_configuration::Divide_value::MAX;
		     div && result.freq_khz < TIMER_MIN_TICKS_PER_MS; div--) {

			if (!div) {
				raw("Failed to calibrate Local APIC frequency");
				return { 0, 1 };
			}
			write<Divide_configuration::Divide_value>((uint8_t)div);

			write<Tmr_initial>(~0U);

			/* Calculate timer frequency */
			result.freq_khz = calibration_fn();
			result.div      = div;

			write<Tmr_initial>(0);
		}

		return result;
	}

	Local_apic(addr_t const addr) : Mmio({(char*)addr, Mmio::SIZE}) {}
};

#endif /* _SRC__INCLUDE__HW__SPEC__X86_64__APIC_H_ */
