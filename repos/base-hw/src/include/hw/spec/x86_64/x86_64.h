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
	class  Vendor;
	struct Tsc;
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


struct Hw::Tsc
{
	static Genode::uint64_t rdtsc()
	{
		Genode::uint32_t low, high;
		asm volatile("rdtsc" : "=a"(low), "=d"(high));
		return (Genode::uint64_t)(high) << 32 | low;
	}

	static bool invariant_tsc()
	{
		using Cpu = Hw::X86_64_cpu;

		Cpu::Cpuid_80000007_eax::access_t eax = Cpu::Cpuid_80000007_eax::read();
		return Cpu::Cpuid_80000007_eax::Invariant_tsc::get(eax);
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
