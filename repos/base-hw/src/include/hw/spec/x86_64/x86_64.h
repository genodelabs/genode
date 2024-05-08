/*
 * \brief  Definitions common to all x86_64 CPUs
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2017-04-10
 */

/*
 * Copyright (C) 2017-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__X86_64__X86_64_H_
#define _SRC__LIB__HW__SPEC__X86_64__X86_64_H_

#include <base/log.h>
#include <base/stdint.h>
#include <hw/spec/x86_64/cpu.h>

namespace Hw {
	struct Cpu_memory_map;
	struct Virtualization_support;
	class Vendor;
	class Lapic;
}


struct Hw::Cpu_memory_map
{
	enum {
		MMIO_IOAPIC_BASE = 0xfec00000,
		MMIO_IOAPIC_SIZE = 0x1000,
		LAPIC_SIZE = 0xe34,
	};

	static Genode::addr_t lapic_phys_base()
	{
		Hw::X86_64_cpu::IA32_apic_base::access_t msr_apic_base;
		msr_apic_base = Hw::X86_64_cpu::IA32_apic_base::read();
		return Hw::X86_64_cpu::IA32_apic_base::Base::masked(msr_apic_base);
	}
};

class Hw::Vendor
{
private:
	static constexpr char const *const vendor_string[] = {
		"GenuineIntel",
		"AuthenticAMD",
		"KVMKVMKVM",
		"Microsoft Hv",
		"VMwareVMware",
		"XenVMMXenVMM"
		"Unknown",
	};

public:
	enum Vendor_id {
		INTEL,
		AMD,
		KVM,
		MICROSOFT,
		VMWARE,
		XEN,
		UNKNOWN,
	};

	static Vendor_id get_vendor_id()
	{
		using Cpu = Hw::X86_64_cpu;
		using Genode::uint32_t;

		uint32_t ebx = static_cast<uint32_t>(Cpu::Cpuid_0_ebx::read());
		uint32_t ecx = static_cast<uint32_t>(Cpu::Cpuid_0_ecx::read());
		uint32_t edx = static_cast<uint32_t>(Cpu::Cpuid_0_edx::read());

		Genode::size_t v;
		for (v = 0; v <= Vendor_id::UNKNOWN; ++v)
			if (*reinterpret_cast<uint32_t const *>(vendor_string[v] +
			                                      0) == ebx &&
			    *reinterpret_cast<uint32_t const *>(vendor_string[v] +
			                                      4) == edx &&
			    *reinterpret_cast<uint32_t const *>(vendor_string[v] +
			                                      8) == ecx)
				break;

		return Vendor_id(v);
	}

	static unsigned get_family()
	{
		using Cpu = Hw::X86_64_cpu;

		Cpu::Cpuid_1_eax::access_t eax = Cpu::Cpuid_1_eax::read();
		return ((eax >> 8 & 0xf) + (eax >> 20 & 0xff)) & 0xff;
	}

	static unsigned get_model()
	{
		using Cpu = Hw::X86_64_cpu;

		Cpu::Cpuid_1_eax::access_t eax = Cpu::Cpuid_1_eax::read();
		return ((eax >> 4 & 0xf) + (eax >> 12 & 0xf0)) & 0xff;
	}
};


class Hw::Lapic
{
      private:
	static bool _has_tsc_dl()
	{
		using Cpu = Hw::X86_64_cpu;

		Cpu::Cpuid_1_ecx::access_t ecx = Cpu::Cpuid_1_ecx::read();
		return (bool)Cpu::Cpuid_1_ecx::Tsc_deadline::get(ecx);
	}

	/*
	 * Adapted from Christian Prochaska's and Alexander Boettcher's
	 * implementation for Nova.
	 *
	 * For details, see Vol. 3B of the Intel SDM (September 2023):
	 * 20.7.3 Determining the Processor Base Frequency
	 */
	static unsigned _read_tsc_freq()
	{
		using Cpu = Hw::X86_64_cpu;

		if (Vendor::get_vendor_id() != Vendor::INTEL)
			return 0;

		unsigned const model  = Vendor::get_model();
		unsigned const family = Vendor::get_family();

		enum
		{
			Cpu_id_clock     = 0x15,
			Cpu_id_base_freq = 0x16
		};

		Cpu::Cpuid_0_eax::access_t eax_0 = Cpu::Cpuid_0_eax::read();

		/*
		 * If CPUID leaf 15 is available, return the frequency reported there.
		 */
		if (eax_0 >= Cpu_id_clock) {
			Cpu::Cpuid_15_eax::access_t eax_15 = Cpu::Cpuid_15_eax::read();
			Cpu::Cpuid_15_ebx::access_t ebx_15 = Cpu::Cpuid_15_ebx::read();
			Cpu::Cpuid_15_ecx::access_t ecx_15 = Cpu::Cpuid_15_ecx::read();

			if (eax_15 && ebx_15) {
				if (ecx_15)
					return static_cast<unsigned>(
						((Genode::uint64_t)(ecx_15) * ebx_15) / eax_15 / 1000
						);

				if (family == 6) {
					if (model == 0x5c) /* Goldmont */
						return static_cast<unsigned>((19200ull * ebx_15) / eax_15);
					if (model == 0x55) /* Xeon */
						return static_cast<unsigned>((25000ull * ebx_15) / eax_15);
				}

				if (family >= 6)
					return static_cast<unsigned>((24000ull * ebx_15) / eax_15);
			}
		}


		/*
		 * Specific methods for family 6 models
		 */
		if (family == 6) {
			unsigned freq_tsc = 0U;

			if (model == 0x2a ||
			    model == 0x2d || /* Sandy Bridge */
			    model >= 0x3a) /* Ivy Bridge and later */
			{
				Cpu::Platform_info::access_t platform_info = Cpu::Platform_info::read();
				Genode::uint64_t ratio = Cpu::Platform_info::Ratio::get(platform_info);
				freq_tsc =  static_cast<unsigned>(ratio * 100000);
			} else if (model == 0x1a ||
				   model == 0x1e ||
				   model == 0x1f ||
				   model == 0x2e || /* Nehalem */
				   model == 0x25 ||
				   model == 0x2c ||
				   model == 0x2f)   /* Xeon Westmere */
				{
					Cpu::Platform_info::access_t platform_info = Cpu::Platform_info::read();
					Genode::uint64_t ratio = Cpu::Platform_info::Ratio::get(platform_info);
					freq_tsc = static_cast<unsigned>(ratio * 133330);
				} else if (model == 0x17 || model == 0xf) { /* Core 2 */
					Cpu::Fsb_freq::access_t fsb_freq = Cpu::Fsb_freq::read();
					Genode::uint64_t freq_bus = Cpu::Fsb_freq::Speed::get(fsb_freq);

					switch (freq_bus) {
						case 0b101: freq_bus = 100000; break;
						case 0b001: freq_bus = 133330; break;
						case 0b011: freq_bus = 166670; break;
						case 0b010: freq_bus = 200000; break;
						case 0b000: freq_bus = 266670; break;
						case 0b100: freq_bus = 333330; break;
						case 0b110: freq_bus = 400000; break;
						default:    freq_bus = 0;      break;
					}

					Cpu::Platform_id::access_t platform_id = Cpu::Platform_id::read();
					Genode::uint64_t ratio = Cpu::Platform_id::Bus_ratio::get(platform_id);

					freq_tsc = static_cast<unsigned>(freq_bus * ratio);
			}

			if (!freq_tsc)
				Genode::warning("TSC: family 6 Intel platform info reports bus frequency of 0");
			else
				return freq_tsc;
		}


		/*
		 * Finally, using Processor Frequency Information for a rough estimate
		 */
		if (eax_0 >= Cpu_id_base_freq) {
			Cpu::Cpuid_16_eax::access_t base_mhz = Cpu::Cpuid_16_eax::read();

			if (base_mhz) {
				Genode::warning("TSC: using processor base frequency: ", base_mhz, " MHz");
				return base_mhz * 1000;
			} else {
				Genode::warning("TSC: CPUID reported processor base frequency of 0");
			}
		}

		return 0;
	}

	static unsigned _measure_tsc_freq()
	{
		const unsigned Tsc_fixed_value = 2400;

		Genode::warning("TSC: calibration not yet implemented, using fixed value of ", Tsc_fixed_value, " MHz");
		/* TODO: implement TSC calibration on AMD */
		return Tsc_fixed_value * 1000;
	}

	public:
		static Genode::uint64_t rdtsc()
		{
			Genode::uint32_t low, high;
			asm volatile("rdtsc" : "=a"(low), "=d"(high));
			return (Genode::uint64_t)(high) << 32 | low;
		}

		static bool invariant_tsc()
		{
			using Cpu = Hw::X86_64_cpu;

			Cpu::Cpuid_80000007_eax::access_t eax =
		    Cpu::Cpuid_80000007_eax::read();
			return Cpu::Cpuid_80000007_eax::Invariant_tsc::get(eax);
		}

		static unsigned tsc_freq()
		{
			unsigned freq = _read_tsc_freq();
			if (freq)
				return freq;
			else
				return _measure_tsc_freq();
		}
};

struct Hw::Virtualization_support
{
	using Cpu = Hw::X86_64_cpu;
	static bool has_svm()
	{
		/*
		 * Check for vendor because the CPUID bit checked for SVM is reserved
		 * on Intel.
		 */
		if (Hw::Vendor::get_vendor_id() != Hw::Vendor::AMD)
			return false;

		Hw::X86_64_cpu::Cpuid_80000001_ecx::access_t cpuid_svm =
				Hw::X86_64_cpu::Cpuid_80000001_ecx::read();

		if (Hw::X86_64_cpu::Cpuid_80000001_ecx::Svm::get(cpuid_svm) != 0) {
			Hw::X86_64_cpu::Amd_vm_cr::access_t amd_vm_cr =
				Hw::X86_64_cpu::Amd_vm_cr::read();

			if (Hw::X86_64_cpu::Amd_vm_cr::Svmdis::get(amd_vm_cr) == 0)
				return true;
		}

		return false;
	}

	static bool has_vmx()
	{
		if (Hw::Vendor::get_vendor_id() != Hw::Vendor::INTEL)
			return false;

		Cpu::Cpuid_1_ecx::access_t ecx = Cpu::Cpuid_1_ecx::read();
		if (!Cpu::Cpuid_1_ecx::Vmx::get(ecx))
			return false;

		/* Check if VMX feature is off and locked */
		Cpu::Ia32_feature_control::access_t feature_control =
			Cpu::Ia32_feature_control::read();
		if (!Cpu::Ia32_feature_control::Vmx_no_smx::get(feature_control) &&
		     Cpu::Ia32_feature_control::Lock::get(feature_control))
			return false;

		return true;
	}
};

#endif /* _SRC__LIB__HW__SPEC__X86_64__X86_64_H_ */
