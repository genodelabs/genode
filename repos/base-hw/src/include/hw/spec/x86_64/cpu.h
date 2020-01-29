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

namespace Hw { struct X86_64_cpu; }

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

	X86_64_CPUID_REGISTER(Cpuid_1_edx, 1, edx,
		struct Pat : Bitfield<16, 1> { };
	);
};

#endif /* _SRC__LIB__HW__SPEC__X86_64__CPU_H_ */
