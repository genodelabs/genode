/*
 * \brief  x86_64 CPU definitions
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2017-04-07
 */

/*
 * Copyright (C) 2017-2024 Genode Labs GmbH
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


	/* Non-architectural MSR used to make lfence serializing */
	X86_64_MSR_REGISTER(Amd_lfence, 0xC0011029,
		struct Enable_dispatch_serializing : Bitfield<1, 1> { }; /* Enable lfence dispatch serializing */
	)

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
		struct Lme  : Bitfield< 8, 1> { }; /* Long Mode Enable */
		struct Lma  : Bitfield<10, 1> { }; /* Long Mode Active */
		struct Svme : Bitfield<12, 1> { }; /* Secure Virtual Machine Enable */
	);

	/* Map of BASE Address of FS */
	X86_64_MSR_REGISTER(Ia32_fs_base, 0xC0000100);

	/* Map of BASE Address of GS */
	X86_64_MSR_REGISTER(Ia32_gs_base, 0xC0000101);

	/* System Call Target Address */
	X86_64_MSR_REGISTER(Ia32_star, 0xC0000081);

	/* IA-32e Mode System Call Target Address */
	X86_64_MSR_REGISTER(Ia32_lstar, 0xC0000082);

	/* IA-32e Mode System Call Target Address */
	X86_64_MSR_REGISTER(Ia32_cstar, 0xC0000083);

	/* System Call Flag Mask */
	X86_64_MSR_REGISTER(Ia32_fmask, 0xC0000084);

	/* Swap Target of BASE Address of GS */
	X86_64_MSR_REGISTER(Ia32_kernel_gs_base, 0xC0000102);

	/* See Vol. 4, Table 2-2 of the Intel SDM */
	X86_64_MSR_REGISTER(Ia32_feature_control, 0x3A,
		struct Lock       : Bitfield< 0, 0> { }; /* VMX Lock */
		struct Vmx_no_smx : Bitfield< 2, 2> { }; /* Enable VMX outside SMX */
	);

	/*
	 * Auxiliary TSC register
	 * For details, see Vol. 3B of the Intel SDM (September 2023):
	 * 18.17.2 IA32_TSC_AUX Register and RDTSCP Support
	 */
	X86_64_MSR_REGISTER(Ia32_tsc_aux, 0xc0000103);

	/*
	 * Reporting Register of Basic VMX Capabilities
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.1 Basic VMX Information
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_basic, 0x480,
		struct Rev             : Bitfield< 0,31> { }; /* VMCS revision */
		struct Clear_controls  : Bitfield<55, 1> { }; /* VMCS controls may be cleared, see A.2 */
	);

	/*
	 * Capability Reporting Register of Pin-Based VM-Execution Controls
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.3.1 Pin-Based VM-Execution Controls
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_pinbased_ctls, 0x481,
		struct Allowed_0_settings : Bitfield< 0,32> { }; /* allowed 0-settings */
		struct Allowed_1_settings : Bitfield<32,32> { }; /* allowed 1-settings */
	);

	/*
	 * Capability Reporting Register of Pin-Based VM-Execution Flex Controls
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.3.1 Pin-Based VM-Execution Controls
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_true_pinbased_ctls, 0x48D,
		struct Allowed_0_settings : Bitfield< 0,32> { }; /* allowed 0-settings */
		struct Allowed_1_settings : Bitfield<32,32> { }; /* allowed 1-settings */
	);

	/*
	 * Capability Reporting Register of Primary Processor-Based VM-Execution Controls
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.3.2 Primary Processor-Based VM-Execution Controls
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_procbased_ctls, 0x482,
		struct Allowed_0_settings : Bitfield< 0,32> { }; /* allowed 0-settings */
		struct Allowed_1_settings : Bitfield<32,32> { }; /* allowed 1-settings */
	);

	/*
	 * Capability Reporting Register of Primary Processor-Based VM-Execution Flex Controls
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.3.2 Primary Processor-Based VM-Execution Controls
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_true_procbased_ctls, 0x48E,
		struct Allowed_0_settings : Bitfield< 0,32> { }; /* allowed 0-settings */
		struct Allowed_1_settings : Bitfield<32,32> { }; /* allowed 1-settings */
	);

	/*
	 * Capability Reporting Register of Primary VM-Exit Controls
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.4.1 Primary VM-Exit Controls
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_exit_ctls, 0x483,
		struct Allowed_0_settings : Bitfield< 0,32> { }; /* allowed 0-settings */
		struct Allowed_1_settings : Bitfield<32,32> { }; /* allowed 1-settings */
	);

	/*
	 * Capability Reporting Register of VM-Exit Flex Controls
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.4.1 Primary VM-Exit Controls
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_true_exit_ctls, 0x48F,
		struct Allowed_0_settings : Bitfield< 0,32> { }; /* allowed 0-settings */
		struct Allowed_1_settings : Bitfield<32,32> { }; /* allowed 1-settings */
	);

	/*
	 * Capability Reporting Register of VM-Entry Controls
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.5 VM-Entry Controls
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_entry_ctls, 0x484,
		struct Allowed_0_settings : Bitfield< 0,32> { }; /* allowed 0-settings */
		struct Allowed_1_settings : Bitfield<32,32> { }; /* allowed 1-settings */
	);

	/*
	 * Capability Reporting Register of VM-Entry Flex Controls
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.5 VM-Entry Controls
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_true_entry_ctls, 0x490,
		struct Allowed_0_settings : Bitfield< 0,32> { }; /* allowed 0-settings */
		struct Allowed_1_settings : Bitfield<32,32> { }; /* allowed 1-settings */
	);

	/*
	 * Capability Reporting Register of Secondary Processor-Based VM-Execution Controls
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.3.3 Secondary Processor-Based VM-Execution Controls
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_procbased_ctls2, 0x48B,
		struct Allowed_0_settings : Bitfield< 0,32> { }; /* allowed 0-settings */
		struct Allowed_1_settings : Bitfield<32,32> { }; /* allowed 1-settings */
	);

	/*
	 * Capability Reporting Register of CR0 Bits Fixed to 0
	 * [sic] in fact, bits reported here need to be 1
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.7 VMX-Fixed Bits in CR0
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_cr0_fixed0, 0x486);

	/*
	 * Capability Reporting Register of CR0 Bits Fixed to 1
	 * [sic] in fact, bits *NOT* reported here need to be 0
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.7 VMX-Fixed Bits in CR0
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_cr0_fixed1, 0x487);

	/*
	 * Capability Reporting Register of CR5 Bits Fixed to 0
	 * [sic] in fact, bits reported here need to be 1
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.8 VMX-Fixed Bits in CR4
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_cr4_fixed0, 0x488);

	/*
	 * Capability Reporting Register of CR4 Bits Fixed to 1
	 * [sic] in fact, bits *NOT* reported here need to be 0
	 * For details, see Vol. 3D of the Intel SDM (September 2023):
	 * A.8 VMX-Fixed Bits in CR4
	 */
	X86_64_MSR_REGISTER(Ia32_vmx_cr4_fixed1, 0x489);


	X86_64_CPUID_REGISTER(Cpuid_0_eax, 0, eax);
	X86_64_CPUID_REGISTER(Cpuid_0_ebx, 0, ebx);
	X86_64_CPUID_REGISTER(Cpuid_0_ecx, 0, ecx);
	X86_64_CPUID_REGISTER(Cpuid_0_edx, 0, edx);

	X86_64_CPUID_REGISTER(Cpuid_1_eax, 1, eax);

	X86_64_CPUID_REGISTER(Cpuid_1_ebx, 1, ebx,
		struct Apic_id : Bitfield<24, 8> { };
	);

	X86_64_CPUID_REGISTER(Cpuid_1_ecx, 1, ecx,
		struct Vmx          : Bitfield< 5, 1> { };
		struct Tsc_deadline : Bitfield<24, 1> { };
	);

	X86_64_CPUID_REGISTER(Cpuid_1_edx, 1, edx,
		struct Pat : Bitfield<16, 1> { };
	);

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
