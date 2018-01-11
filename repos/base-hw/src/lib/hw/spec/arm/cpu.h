/*
 * \brief  CPU definitions for ARM
 * \author Stefan Kalkowski
 * \date   2017-02-02
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__ARM__CPU_H_
#define _SRC__LIB__HW__SPEC__ARM__CPU_H_

#include <hw/spec/arm/register_macros.h>

namespace Hw { struct Arm_cpu; }

struct Hw::Arm_cpu
{
	/***************************************
	 ** System Coprocessor 15 Definitions **
	 ***************************************/

	/* Main ID Register */
	ARM_CP15_REGISTER_32BIT(Midr, c0, c0, 0, 0);

	/* Cache Type Register */
	ARM_CP15_REGISTER_32BIT(Ctr, c0, c0, 0, 1);

	/* Multiprocessor Affinity Register */
	ARM_CP15_REGISTER_32BIT(Mpidr, c0, c0, 0, 5,
		struct Aff_0 : Bitfield<0, 8> { };  /* affinity value 0 */
		struct Me    : Bitfield<31, 1> { }; /* multiprocessing extension */
	);

	/* System Control Register */
	ARM_CP15_REGISTER_32BIT(Sctlr, c1, c0, 0, 0,
		struct M : Bitfield<0,1>  { }; /* enable MMU */
		struct A : Bitfield<1,1>  { }; /* enable alignment checks */
		struct C : Bitfield<2,1>  { }; /* enable data cache */
		struct I : Bitfield<12,1> { }; /* enable instruction caches */
		struct Z : Bitfield<11,1> { }; /* enable program flow prediction */
		struct V : Bitfield<13,1> { }; /* select exception entry */
	);

	/* Auxiliary Control Register */
	ARM_CP15_REGISTER_32BIT(Actlr, c1, c0, 0, 1);

	/* Coprocessor Access Control Register */
	ARM_CP15_REGISTER_32BIT(Cpacr, c1, c0, 0, 2,
		struct Cp10 : Bitfield<20, 2> { };
		struct Cp11 : Bitfield<22, 2> { };
	);

	/* Hyp System Control Register */
	ARM_CP15_REGISTER_32BIT(Hsctlr, c1, c0, 4, 0);

	/* Secure Configuration Register */
	ARM_CP15_REGISTER_32BIT(Scr, c1, c1, 0, 0,
		struct Ns  : Bitfield<0, 1> { }; /* not secure               */
		struct Fw  : Bitfield<4, 1> { }; /* F bit writeable          */
		struct Aw  : Bitfield<5, 1> { }; /* A bit writeable          */
		struct Scd : Bitfield<7, 1> { }; /* smc call disable         */
		struct Hce : Bitfield<8, 1> { }; /* hyp call enable          */
		struct Sif : Bitfield<9, 1> { }; /* secure instruction fetch */
	);

	/* Secure Debug Enable Register */
	ARM_CP15_REGISTER_32BIT(Sder, c1, c1, 0, 1);

	/* Non-Secure Access Control Register */
	ARM_CP15_REGISTER_32BIT(Nsacr, c1, c1, 0, 2,
		struct Cpnsae10 : Bitfield<10, 1> { }; /* Co-processor 10 access */
		struct Cpnsae11 : Bitfield<11, 1> { }; /* Co-processor 11 access */
		struct Ns_smp   : Bitfield<18,1> { };
	);

	/* Hyp Coprocessor Trap Register */
	ARM_CP15_REGISTER_32BIT(Hcptr, c1, c1, 4, 2,
		template <unsigned N> struct Tcp : Bitfield<N, 1> {};
		struct Tase  : Bitfield<15, 1> { };
		struct Tta   : Bitfield<20, 1> { };
		struct Tcpac : Bitfield<31, 1> { };
	);

	/**
	 * Common translation table base register
	 */
	struct Ttbr : Genode::Register<32>
	{
		enum Memory_region { CACHEABLE = 1 };

		struct C   : Bitfield<0,1> { };    /* inner cacheable */
		struct S   : Bitfield<1,1> { };    /* shareable */
		struct Rgn : Bitfield<3,2> { };    /* outer cachable mode */
		struct Nos : Bitfield<5,1> { };    /* not outer shareable */
		struct Ba  : Bitfield<14, 18> { }; /* translation table base */


		/*********************************
		 *  Multiprocessing Extensions  **
		 *********************************/

		struct Irgn_1 : Bitfield<0,1> { };
		struct Irgn_0 : Bitfield<6,1> { };
		struct Irgn : Genode::Bitset_2<Irgn_0, Irgn_1> { }; /* inner cache mode */
	};

	struct Ttbr_64bit : Genode::Register<64>
	{
		struct Ba   : Bitfield<4, 35> { }; /* translation table base */
		struct Asid : Bitfield<48,8> { };
	};

	/* Translation Table Base Control Register */
	ARM_CP15_REGISTER_32BIT(Ttbcr, c2, c0, 0, 2,

		/***************************************
		 ** Large Physical Address Extensions **
		 ***************************************/

		struct T0sz  : Bitfield<0,  3> { };
		struct Irgn0 : Bitfield<8,  2> { };
		struct Orgn0 : Bitfield<10, 2> { };
		struct Sh0   : Bitfield<12, 2> { };
		struct T1sz  : Bitfield<16, 3> { };
		struct Irgn1 : Bitfield<24, 2> { };
		struct Orgn1 : Bitfield<26, 2> { };
		struct Sh1   : Bitfield<28, 2> { };
		struct Eae   : Bitfield<31, 1> { }; /* extended address enable */
	);

	/* Translation Table Base Register 0 */
	ARM_CP15_REGISTER_32BIT(Ttbr0, c2, c0, 0, 0);
	ARM_CP15_REGISTER_64BIT(Ttbr0_64bit, c2, 0);

	/* Translation Table Base Register 1 */
	ARM_CP15_REGISTER_32BIT(Ttbr1, c2, c0, 0, 1);
	ARM_CP15_REGISTER_64BIT(Ttbr1_64bit, c2, 1);

	/* Hyp Translation Control Register */
	ARM_CP15_REGISTER_32BIT(Htcr, c2, c0, 4, 2);

	/* Hyp Translation Table Base Register */
	ARM_CP15_REGISTER_64BIT(Httbr_64bit, c2, 4);

	/* Virtualization Translation Control Register */
	ARM_CP15_REGISTER_32BIT(Vtcr, c2, c1, 4, 2,
		struct Sl0 : Bitfield<6,2> {}; /* starting level for table walks */
	);

	/* Domain Access Control Register */
	ARM_CP15_REGISTER_32BIT(Dacr,  c3,  c0, 0, 0,
		struct D0 : Bitfield<0,2> { }; /* access mode for domain 0 */
	);

	/**
	 * Common fault status register
	 */
	struct Fsr : Genode::Register<32>
	{
		struct Fs_0 : Bitfield<0, 4> { };
		struct Fs_1 : Bitfield<10, 1> { };
		struct Fs : Genode::Bitset_2<Fs_0, Fs_1> { }; /* fault status */
	};

	/* Data Fault Status Register */
	ARM_CP15_REGISTER_32BIT(Dfsr, c5, c0, 0, 0,
		struct Wnr : Bitfield<11, 1> { }; /* write not read bit */
	);

	/* Instruction Fault Status Register */
	ARM_CP15_REGISTER_32BIT(Ifsr, c5, c0, 0, 1);

	/* Data Fault Address Register */
	ARM_CP15_REGISTER_32BIT(Dfar, c6, c0, 0, 0);

	/* Instruction Fault Address Register */
	ARM_CP15_REGISTER_32BIT(Ifar, c6, c0, 0, 2);

	/* Invalidate instruction cache line by MVA to PoU */
	ARM_CP15_REGISTER_32BIT(Icimvau, c7, c5,  0, 1);

	/* Branch predictor invalidate all */
	ARM_CP15_REGISTER_32BIT(Bpiall, c7, c5,  0, 6);

	/* Data Cache Clean and Invalidate by MVA to PoC */
	ARM_CP15_REGISTER_32BIT(Dccimvac, c7, c14, 0, 1);

	/* Invalidate entire unified TLB */
	ARM_CP15_REGISTER_32BIT(Tlbiall, c8, c7, 0, 0);

	/* Invalidate unified TLB by ASID */
	ARM_CP15_REGISTER_32BIT(Tlbiasid, c8, c7, 0, 2);

	/* Memory Attribute Indirection Register 0 */
	ARM_CP15_REGISTER_32BIT(Mair0, c10, c2, 0, 0,
		struct Attr0 : Bitfield<0,  8> { };
		struct Attr1 : Bitfield<8,  8> { };
		struct Attr2 : Bitfield<16, 8> { };
		struct Attr3 : Bitfield<24, 8> { };
	);

	/* Hyp Memory Attribute Indirection Register 0 */
	ARM_CP15_REGISTER_32BIT(Hmair0, c10, c2, 4, 0);

	/* Monitor Vector Base Address Register */
	ARM_CP15_REGISTER_32BIT(Mvbar, c12, c0, 0, 1);

	/* Hyp Vector Base Address Register */
	ARM_CP15_REGISTER_32BIT(Hvbar, c12, c0, 4, 0);

	/* Context ID Register */
	ARM_CP15_REGISTER_32BIT(Cidr, c13, c0, 0, 1);

	/* Counter Frequency register */
	ARM_CP15_REGISTER_32BIT(Cntfrq, c14, c0, 0, 0);

	/******************************
	 ** Program status registers **
	 ******************************/

	/**
	 * Common program status register
	 */
	struct Psr : Genode::Register<32>
	{
		/*
		 * CPU mode
		 */
		struct M : Bitfield<0,5>
		{
			enum {
				USR = 16,
				SVC = 19,
				MON = 22,
				HYP = 26,
				SYS = 31,
			};
		};

		struct F : Bitfield<6,1> { }; /* FIQ disable */
		struct I : Bitfield<7,1> { }; /* IRQ disable */
		struct A : Bitfield<8,1> { }; /* async. abort disable */
	};

	ARM_BANKED_REGISTER(Cpsr, cpsr);


	/**********************************
	 ** Cache maintainance functions **
	 **********************************/

	static void clean_invalidate_data_cache();
	static void invalidate_data_cache();
};

#endif /* _SRC__LIB__HW__SPEC__ARM__CPU_H_ */
