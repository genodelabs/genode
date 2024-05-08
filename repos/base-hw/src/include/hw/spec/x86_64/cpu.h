/*
 * \brief  x86_64 CPU definitions
 * \author Stefan Kalkowski
 * \date   2017-04-07
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__X86_64__CPU_H_
#define _SRC__LIB__HW__SPEC__X86_64__CPU_H_

#include <hw/spec/x86_64/register_macros.h>

namespace Hw { struct X86_64_cpu; struct Suspend_type; }


/*
 * The intended sleep state S0 ... S5. The values are read out by an
 * ACPI AML component and are of type TYP_SLPx as described in the
 * ACPI specification, e.g. TYP_SLPa and TYP_SLPb. The values differ
 * between different PC systems/boards.
 */
struct Hw::Suspend_type {
	Genode::uint8_t typ_a;
	Genode::uint8_t typ_b;
};


struct Hw::X86_64_cpu
{
	X86_64_CR_REGISTER(Cr0, cr0,
		struct Pe : Bitfield< 0, 1> { }; /* Protection Enable   */
		struct Mp : Bitfield< 1, 1> { }; /* Monitor Coprocessor */
		struct Em : Bitfield< 2, 1> { }; /* Emulation           */
		struct Ts : Bitfield< 3, 1> { }; /* Task Switched       */
		struct Et : Bitfield< 4, 1> { }; /* Extension Type      */
		struct Ne : Bitfield< 5, 1> { }; /* Numeric Error       */
		struct Wp : Bitfield<16, 1> { }; /* Write Protect       */
		struct Am : Bitfield<18, 1> { }; /* Alignment Mask      */
		struct Nw : Bitfield<29, 1> { }; /* Not Write-through   */
		struct Cd : Bitfield<30, 1> { }; /* Cache Disable       */
		struct Pg : Bitfield<31, 1> { }; /* Paging              */
	);

	/**
	 * Control register 2: Page-fault linear address
	 *
	 * See Intel SDM Vol. 3A, section 2.5.
	 */
	X86_64_CR_REGISTER(Cr2, cr2,
		struct Addr : Bitfield<0, 63> { };
	);

	/**
	 * Control register 3: Page-Directory base register
	 *
	 * See Intel SDM Vol. 3A, section 2.5.
	 */
	X86_64_CR_REGISTER(Cr3, cr3,
		struct Pwt : Bitfield<3,1> { };    /* Page-level write-through    */
		struct Pcd : Bitfield<4,1> { };    /* Page-level cache disable    */
		struct Pdb : Bitfield<12, 36> { }; /* Page-directory base address */
	);

	X86_64_CR_REGISTER(Cr4, cr4,
		struct Vme        : Bitfield< 0, 1> { }; /* Virtual-8086 Mode Extensions */
		struct Pvi        : Bitfield< 1, 1> { }; /* Protected-Mode Virtual IRQs */
		struct Tsd        : Bitfield< 2, 1> { }; /* Time Stamp Disable */
		struct De         : Bitfield< 3, 1> { }; /* Debugging Exceptions */
		struct Pse        : Bitfield< 4, 1> { }; /* Page Size Extensions */
		struct Pae        : Bitfield< 5, 1> { }; /* Physical Address Extension */
		struct Mce        : Bitfield< 6, 1> { }; /* Machine-Check Enable */
		struct Pge        : Bitfield< 7, 1> { }; /* Page Global Enable */
		struct Pce        : Bitfield< 8, 1> { }; /* Performance-Monitoring Counter
		                                            Enable*/
		struct Osfxsr     : Bitfield< 9, 1> { }; /* OS Support for FXSAVE and
		                                            FXRSTOR instructions*/
		struct Osxmmexcpt : Bitfield<10, 1> { }; /* OS Support for Unmasked
		                                            SIMD/FPU Exceptions */
		struct Vmxe       : Bitfield<13, 1> { }; /* VMX Enable */
		struct Smxe       : Bitfield<14, 1> { }; /* SMX Enable */
		struct Fsgsbase   : Bitfield<16, 1> { }; /* FSGSBASE-Enable */
		struct Pcide      : Bitfield<17, 1> { }; /* PCIDE Enable */
		struct Osxsave    : Bitfield<18, 1> { }; /* XSAVE and Processor Extended
		                                            States-Enable */
		struct Smep       : Bitfield<20, 1> { }; /* SMEP Enable */
		struct Smap       : Bitfield<21, 1> { }; /* SMAP Enable */
	);

	X86_64_MSR_REGISTER(IA32_apic_base,  0x1b,
		struct Bsp   : Bitfield<  8,  1> { }; /* Bootstrap processor */
		struct Lapic : Bitfield< 11,  1> { }; /* Enable/disable local APIC */
		struct Base  : Bitfield< 12, 24> { }; /* Base address of APIC registers */
	);

	X86_64_MSR_REGISTER(IA32_pat, 0x277,
		struct Pa1 : Bitfield <8, 3> {
			enum { WRITE_COMBINING = 0b001 };
		};
	);

	X86_64_MSR_REGISTER(Amd_vm_syscvg, 0xC0010010,
		struct Nested_paging : Bitfield< 0, 1> { }; /* Enable nested paging */
		struct Sev : Bitfield< 1, 1> { }; /* Enable Secure Encrypted Virtualization */
		struct Enc_state : Bitfield< 2, 1> { }; /* Enable Encrypted State for Secure Encrypted Virtualization */
	);

	X86_64_MSR_REGISTER(Amd_vm_cr, 0xC0010114,
		struct Svmdis : Bitfield< 4, 1> { }; /* SVM disabled */
	);

	/* AMD host save physical address */
	X86_64_MSR_REGISTER(Amd_vm_hsavepa, 0xC0010117);

	X86_64_MSR_REGISTER(Platform_id, 0x17,
		struct Bus_ratio : Bitfield<8, 5> { }; /* Bus ratio on Core 2, see SDM 19.7.3 */
	);

	X86_64_MSR_REGISTER(Platform_info, 0xCE,
		struct Ratio : Bitfield< 8, 8> { }; /* Maximum Non-Turbo Ratio (R/O) */
	);

	X86_64_MSR_REGISTER(Fsb_freq, 0xCD,
		struct Speed : Bitfield< 0, 3> { }; /* Scaleable Bus Speed (R/O) */
	);

	X86_64_MSR_REGISTER(Ia32_efer, 0xC0000080,
		struct Svme : Bitfield< 12, 1> { }; /* Secure Virtual Machine Enable */
	);

	/*
	 * Auxiliary TSC register
	 * For details, see Vol. 3B of the Intel SDM:
	 * 17.17.2 IA32_TSC_AUX Register and RDTSCP Support
	 */
	X86_64_MSR_REGISTER(Ia32_tsc_aux, 0xc0000103);

	X86_64_CPUID_REGISTER(Cpuid_0_eax, 0, eax);
	X86_64_CPUID_REGISTER(Cpuid_0_ebx, 0, ebx);
	X86_64_CPUID_REGISTER(Cpuid_0_ecx, 0, ecx);
	X86_64_CPUID_REGISTER(Cpuid_0_edx, 0, edx);

	X86_64_CPUID_REGISTER(Cpuid_1_eax, 1, eax);

	X86_64_CPUID_REGISTER(Cpuid_1_ecx, 1, ecx,
		struct Tsc_deadline : Bitfield<24, 1> { };
	);

	X86_64_CPUID_REGISTER(Cpuid_1_edx, 1, edx,
		struct Pat : Bitfield<16, 1> { };
	);

	/* Number of address space identifiers (ASID) */
	X86_64_CPUID_REGISTER(Amd_nasid, 0x8000000A, ebx);

	X86_64_CPUID_REGISTER(Cpuid_15_eax, 15, eax);
	X86_64_CPUID_REGISTER(Cpuid_15_ebx, 15, ebx);
	X86_64_CPUID_REGISTER(Cpuid_15_ecx, 15, ecx);

	X86_64_CPUID_REGISTER(Cpuid_16_eax, 16, ecx);

	X86_64_CPUID_REGISTER(Cpuid_8000000A_edx, 0x8000000A, edx,
		struct Np : Bitfield<0, 1> { }; /* Nested paging */
	);

	X86_64_CPUID_REGISTER(Cpuid_80000007_eax, 0x80000007, eax,
		struct Invariant_tsc : Bitfield<2, 1> { }; /* Invariant TSC */
	);

	X86_64_CPUID_REGISTER(Cpuid_80000001_ecx, 0x80000001, ecx,
		struct Svm : Bitfield<2, 1> { };
	);

	Suspend_type suspend;
};

#endif /* _SRC__LIB__HW__SPEC__X86_64__CPU_H_ */
