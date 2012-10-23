/*
 * \brief  Simple driver for the ARMv7 core
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ARM_V7__CPU_H_
#define _INCLUDE__ARM_V7__CPU_H_

/* Genode includes */
#include <drivers/board.h>

/* core includes */
#include <arm/cpu.h>

namespace Arm_v7
{
	using namespace Genode;

	/**
	 * ARMv7 core
	 */
	struct Cpu : Arm::Cpu
	{
		/**
		 * Secure configuration register
		 */
		struct Scr : Register<32>
		{
			struct Ns : Bitfield<0, 1> { }; /* not secure */

			/**
			 * Read register value
			 */
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %[v], c1, c1, 0" : [v]"=r"(v) ::);
				return v;
			}
		};

		/**
		 * Non-secure access control register
		 */
		struct Nsacr : Register<32>
		{
			/************************************************
			 ** Coprocessor 0-13 non-secure acccess enable **
			 ************************************************/

			struct Cpnsae10 : Bitfield<10, 1> { };
			struct Cpnsae11 : Bitfield<11, 1> { };
		};

		/**
		 * System control register
		 */
		struct Sctlr : Arm::Cpu::Sctlr
		{
			struct Unused_0 : Bitfield<3,4>  { }; /* shall be ~0 */
			struct Sw       : Bitfield<10,1> { }; /* support SWP and SWPB */
			struct Unused_1 : Bitfield<16,1> { }; /* shall be ~0 */
			struct Ha       : Bitfield<17,1> { }; /* enable HW access flag */
			struct Unused_2 : Bitfield<18,1> { }; /* shall be ~0 */
			struct Unused_3 : Bitfield<22,2> { }; /* shall be ~0 */
			struct Nmfi     : Bitfield<27,1> { }; /* FIQs are non-maskable */
			struct Tre      : Bitfield<28,1> { }; /* remap TEX[2:1] for OS */

			struct Afe : Bitfield<29,1> /* translation access perm. mode */
			{
				enum { FULL_RANGE_OF_PERMISSIONS = 0 };
			};

			struct Te : Bitfield<30,1> { }; /* do exceptions in Thumb state */

			/**
			 * Static base value
			 */
			static access_t base_value()
			{
				return Unused_0::bits(~0) |
				       Unused_1::bits(~0) |
				       Unused_2::bits(~0) |
				       Unused_3::bits(~0);
			}

			/**
			 * Value for the first kernel run
			 */
			static access_t init_phys_kernel()
			{
				return base_value() |
				       Arm::Cpu::Sctlr::init_virt_kernel() |
				       Sw::bits(0) |
				       Ha::bits(0) |
				       Nmfi::bits(0) |
				       Tre::bits(0);
			}

			/**
			 * Value for the switch to virtual mode in kernel
			 */
			static access_t init_virt_kernel()
			{
				return base_value() |
				       Arm::Cpu::Sctlr::init_virt_kernel() |
				       Sw::bits(0) |
				       Ha::bits(0) |
				       Nmfi::bits(0) |
				       Tre::bits(0);
			}
		};

		/**
		 * Translation table base register 0
		 */
		struct Ttbr0 : Arm::Cpu::Ttbr0
		{
			struct Nos : Bitfield<6,1> { }; /* not outer shareable */

			struct Irgn_0 : Bitfield<6,1> /* inner cachable mode */
			{
				enum { NON_CACHEABLE = 0 };
			};

			/**
			 * Value for the switch to virtual mode in kernel
			 *
			 * \param sect_table  pointer to initial section table
			 */
			static access_t init_virt_kernel(addr_t const sect_table)
			{
				return Arm::Cpu::Ttbr0::init_virt_kernel(sect_table) |
				       Nos::bits(0) |
				       Irgn_0::bits(Irgn_0::NON_CACHEABLE);
			}
		};

		/**
		 * Translation table base control register
		 */
		struct Ttbcr : Arm::Cpu::Ttbcr
		{
			struct Pd0 : Bitfield<4,1> { }; /* disable walk for TTBR0 */
			struct Pd1 : Bitfield<5,1> { }; /* disable walk for TTBR1 */

			/**
			 * Value for the switch to virtual mode in kernel
			 */
			static access_t init_virt_kernel()
			{
				return Arm::Cpu::Ttbcr::init_virt_kernel() |
				       Pd0::bits(0) |
				       Pd1::bits(0);
			}
		};

		/**
		 * Switch to the virtual mode in kernel
		 *
		 * \param section_table  section translation table of the initial
		 *                       address space this function switches to
		 * \param process_id     process ID of the initial address space
		 */
		static void init_virt_kernel(addr_t const section_table,
		                             unsigned long const process_id)
		{
			Cidr::write(process_id);
			Dacr::write(Dacr::init_virt_kernel());
			Ttbr0::write(Ttbr0::init_virt_kernel(section_table));
			Ttbcr::write(Ttbcr::init_virt_kernel());
			Sctlr::write(Sctlr::init_virt_kernel());
		}

		/**
		 * Configure this module appropriately for the first kernel run
		 */
		static void init_phys_kernel()
		{
			Psr::write(Psr::init_kernel());
			flush_tlb();
		}

		/**
		 * Wether we are in secure mode
		 */
		static bool secure_mode()
		{
			if (!Board::SECURITY_EXTENSION) return 0;
			return !Cpu::Scr::Ns::get(Cpu::Scr::read());
		}


		/******************************
		 **  Trustzone specific API  **
		 ******************************/

		/**
		 * Set the exception-vector's base-address for the monitor mode
		 * software stack.
		 *
		 * \param addr  address of the exception vector's base
		 */
		static inline void mon_exception_entry_at(addr_t const addr)
		{
			asm volatile ("mcr p15, 0, %[rd], c12, c0, 1" : : [rd] "r" (addr));
		}

		/**
		 * Enable access of co-processors cp10 and cp11 from non-secure mode.
		 */
		static inline void allow_coprocessor_nonsecure()
		{
			Nsacr::access_t rd = Nsacr::Cpnsae10::bits(1) |
			                     Nsacr::Cpnsae11::bits(1);
			asm volatile ("mcr p15, 0, %[rd], c1, c1, 2" : : [rd] "r" (rd));
		}
	};
}


/**************
 ** Arm::Cpu **
 **************/

void Arm::Cpu::flush_data_caches()
{
	/*
	 * FIXME This routine is taken from the ARMv7 reference manual by
	 *       applying the inline-assembly framework and replacing 'Loop1'
	 *       with '1:', 'Loop2' with '2:', 'Loop3' with '3:', 'Skip'
	 *       with '4:' and 'Finished' with '5:'. It might get implemented
	 *       with more beauty.
	 */
	asm volatile (
		"mrc p15, 1, r0, c0, c0, 1\n"   /* read CLIDR into R0 */
		"ands r3, r0, #0x7000000\n"
		"mov r3, r3, lsr #23\n"         /* cache level value (naturally aligned) */
		"beq 5f\n"
		"mov r10, #0\n"

		"1:\n"

		"add r2, r10, r10, lsr #1\n"    /* work out 3 x cachelevel */
		"mov r1, r0, lsr r2\n"          /* bottom 3 bits are the Cache type for this level */
		"and r1, r1, #7\n"              /* get those 3 bits alone */
		"cmp r1, #2\n"
		"blt 4f\n"                      /* no cache or only instruction cache at this level */
		"mcr p15, 2, r10, c0, c0, 0\n"  /* write CSSELR from R10 */
		"isb\n"                         /* ISB to sync the change to the CCSIDR */
		"mrc p15, 1, r1, c0, c0, 0\n"   /* read current CCSIDR to R1 */
		"and r2, r1, #0x7\n"            /* extract the line length field */
		"add r2, r2, #4\n"              /* add 4 for the line length offset (log2 16 bytes) */
		"ldr r4, =0x3ff\n"
		"ands r4, r4, r1, lsr #3\n"     /* R4 is the max number on the way size (right aligned) */
		"clz r5, r4\n"                  /* R5 is the bit position of the way size increment */
		"mov r9, r4\n"                  /* R9 working copy of the max way size (right aligned) */

		"2:\n"

		"ldr r7, =0x00007fff\n"
		"ands r7, r7, r1, lsr #13\n"    /* R7 is the max number of the index size (right aligned) */

		"3:\n"

		"orr r11, r10, r9, lsl r5\n"    /* factor in the way number and cache number into R11 */
		"orr r11, r11, r7, lsl r2\n"    /* factor in the index number */
		"mcr p15, 0, r11, c7, c10, 2\n" /* DCCSW, clean by set/way */
		"subs r7, r7, #1\n"             /* decrement the index */
		"bge 3b\n"
		"subs r9, r9, #1\n"             /* decrement the way number */
		"bge 2b\n"

		"4:\n"

		"add r10, r10, #2\n"            /* increment the cache number */
		"cmp r3, r10\n"
		"bgt 1b\n"
		"dsb\n"

		"5:\n"

		::: "r0", "r1", "r2", "r3", "r4",
			"r5", "r7", "r9", "r10", "r11");
}


Arm::Cpu::Psr::access_t Arm::Cpu::Psr::init_user_with_trustzone()
{
	return M::bits(M::USER) |
	       T::bits(T::ARM) |
	       F::bits(0) |
	       I::bits(1) |
	       A::bits(1) |
	       E::bits(E::LITTLE) |
	       J::bits(J::ARM);
}


#endif /* _INCLUDE__ARM_V7__CPU_H_ */

