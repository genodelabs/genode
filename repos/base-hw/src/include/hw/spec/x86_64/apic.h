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

#include <hw/spec/x86_64/acpi.h>
#include <hw/spec/x86_64/cpu.h>
#include <hw/spec/x86_64/x86_64.h>

/* Genode includes */
#include <drivers/timer/util.h>
#include <util/mmio.h>

namespace Hw {
	using namespace Genode;
	class Msr_mmio_access;
	class Apic;
}

/**
 * This utility helps to distinguish in between MMIO (Local-APIC) and
 * MSR (X2APIC) register accesses. Both hardware interfaces provide a
 * common subset, and can be used in a compliant way.
 * Where MMIO register in general have 16-byte aligned offsets like:
 * 0x100 the corresponding MSR offset is shifted to be 0x10.
 */
class Hw::Msr_mmio_access
{
	friend Register_set_plain_access;

	protected:

		static constexpr addr_t MSR_START = 0x800 ;

		Byte_range_ptr const _range;
		bool const _msr;

		template <typename T>
		void _write(off_t const offset, T const value)
		{
			static_assert(sizeof(T) == sizeof(uint32_t));

			if (_msr) asm volatile ("wrmsr" : : "a" (value), "d" (0),
			                        "c" (MSR_START + (offset>>4)));
			else *(T volatile *)(_range.start + offset) = value;
		}

		template <typename T>
		T _read(off_t const &offset) const
		{
			static_assert(sizeof(T) == sizeof(uint32_t));

			if (!_msr)
				return *(T volatile *)(_range.start + offset);

			T low, high;
			asm volatile ("rdmsr" : "=a" (low),
			              "=d" (high) : "c" (MSR_START + (offset>>4)));
			return low;
		}

	public:

		Msr_mmio_access(Byte_range_ptr const &range, bool const msr)
		:
			_range(range.start, range.num_bytes), _msr(msr) { }

		Byte_range_ptr range_at(off_t offset) const {
			return {_range.start + offset, _range.num_bytes - offset}; }

		Byte_range_ptr range() const { return range_at(0); }

		addr_t base() const { return (addr_t)range().start; }
};


struct Hw::Apic
: Msr_mmio_access, Register_set<Msr_mmio_access, Hw::Cpu_memory_map::LAPIC_SIZE>
{
	static constexpr size_t SIZE = Hw::Cpu_memory_map::LAPIC_SIZE;

	struct Eoi : Register<0x0b0, 32, true> { };

	struct Svr : Register<0x0f0, 32>
	{
		struct APIC_enable : Bitfield<8, 1> { };
	};

	/*
	 * ISR register, see Intel SDM Vol. 3A, section 10.8.4.
	 *
	 * Actually only at offsets of multiple of 0x10 an ISR register
	 * exists. Therefore, this array has to ignore entries with
	 * i % 4 != 0
	 */
	struct Isr : Register_array<0x100, 32, 8*4, 32> { };

	/*
	 * Interrupt control register
	 */
	struct Icr_low : Register<0x300, 32>
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

	struct Icr_high : Register<0x310, 32>
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

	struct Calibration { uint32_t freq_khz; uint8_t div; };

	void end_of_interrupt() { write<Eoi>(0); }

	unsigned get_lowest_active_irq()
	{
		unsigned bit, vec_base = 0;

		for (unsigned i = 0; i < 8*4; i+=4) {
			bit = __builtin_ffs(read<Isr>(i));
			if (bit) {
				return vec_base + bit;
			}
			vec_base += 32;
		}
		return 0;
	}

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

		/**
		 * Unfortunately, the ICR register is slightly different
		 * in between LAPIC and X2APIC, therefore we have to take
		 * care of the two different access forms here explicitely
		 */
		if (!_msr) {
			write<Icr_high::Destination>(destination);
			write<Icr_low>(icr_low);
		} else {
			enum { ICR_MSR_ADDR = 0x830 };
			asm volatile ("wrmsr" :: "a" (icr_low), "d" (destination),
			              "c" (ICR_MSR_ADDR));
		}
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
		if (_msr) Apic_msr::X2apic::set(apic_msr, 1);
		else      Apic_msr::Lapic::set(apic_msr);
		Apic_msr::write(apic_msr);

		if (!read<Svr::APIC_enable>()) write<Svr::APIC_enable>(true);
	}

	void timer_reset_ticks(uint32_t const ticks)
	{
		write<Tmr_initial>(ticks);
	}

	void timer_init(uint8_t const vector, uint8_t const divider)
	{
		/* enable LAPIC timer in one-shot mode */
		write<Tmr_lvt::Vector>(vector);
		write<Tmr_lvt::Delivery>(0);
		write<Tmr_lvt::Mask>(0);
		write<Tmr_lvt::Timer_mode>(0);
		write<Divide_configuration::Divide_value>((uint8_t)divider);
	}

	Calibration timer_calibrate(Hw::Acpi::Fadt &fadt)
	{
		Calibration result { };

		static constexpr uint32_t sleep_ms = 10;

		/* calibrate LAPIC frequency to fullfill our requirements */
		for (uint8_t div = Divide_configuration::Divide_value::MAX;
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

	Apic(addr_t const addr)
	:
		Msr_mmio_access({(char*)addr, SIZE}, X86_64_cpu::x2apic_support()),
		Register_set<Msr_mmio_access, SIZE>(*static_cast<Msr_mmio_access*>(this))
	{ enable(); }
};

#endif /* _SRC__INCLUDE__HW__SPEC__X86_64__APIC_H_ */
