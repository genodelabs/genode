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

#include <hw/spec/x86_64/acpi.h>
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
			enum Mode { FIXED = 0, INIT = 5, STARTUP = 6 };
		};

		struct Delivery_status : Bitfield<12, 1> { };
		struct Level_assert    : Bitfield<14, 1> { };
		struct Dest_shorthand  : Bitfield<18, 2>
		{
			enum Shorthand { NO = 0, ALL_OTHERS = 3 };
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

	void end_of_interrupt() { write<EOI>(0); }

	void send_ipi(uint8_t const vector, uint8_t const destination,
	              Icr_low::Delivery_mode::Mode const mode,
	              Icr_low::Dest_shorthand::Shorthand dest_shorthand)
	{
		/* wait until ready */
		while (read<Icr_low::Delivery_status>())
			asm volatile ("pause":::"memory");

		Icr_low::access_t icr_low = 0;
		Icr_low::Vector::set(icr_low, vector);
		Icr_low::Delivery_mode::set(icr_low, mode);
		Icr_low::Level_assert::set(icr_low);
		Icr_low::Dest_shorthand::set(icr_low, dest_shorthand);

		/* program */
		write<Icr_high::Destination>(destination);
		write<Icr_low>(icr_low);
	}

	void send_ipi_to_all(uint8_t const vector,
	                     Icr_low::Delivery_mode::Mode const mode)
	{
		send_ipi(vector, 0, mode, Icr_low::Dest_shorthand::ALL_OTHERS);
	}

	void enable()
	{
		using Apic_msr = Hw::X86_64_cpu::IA32_apic_base;

		/* we like to use local APIC */
		Apic_msr::access_t apic_msr = Apic_msr::read();
		Apic_msr::Lapic::set(apic_msr);
		Apic_msr::write(apic_msr);

		if (!read<Svr::APIC_enable>()) write<Svr::APIC_enable>(true);
	}

	Calibration calibrate(Hw::Acpi::Fadt &fadt)
	{
		Calibration result { };

		static constexpr uint32_t sleep_ms = 10;

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
			result.div      = div;
			result.freq_khz = fadt.calibrate_freq_khz(sleep_ms, [&] {
				return read<Tmr_current>(); }, true);

			write<Tmr_initial>(0);
		}

		return result;
	}

	Local_apic(addr_t const addr) : Mmio({(char*)addr, Mmio::SIZE}) {}
};

#endif /* _SRC__INCLUDE__HW__SPEC__X86_64__APIC_H_ */
