/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROCESSOR_DRIVER__ARM_V7_H_
#define _PROCESSOR_DRIVER__ARM_V7_H_

/* core includes */
#include <processor_driver/arm.h>
#include <board.h>

/**
 * Helpers that increase readability of MCR and MRC commands
 */
#define READ_CLIDR(rd)   "mrc p15, 1, " #rd ", c0, c0, 1\n"
#define READ_CCSIDR(rd)  "mrc p15, 1, " #rd ", c0, c0, 0\n"
#define WRITE_CSSELR(rs) "mcr p15, 2, " #rs ", c0, c0, 0\n"
#define WRITE_DCISW(rs)  "mcr p15, 0, " #rs ", c7, c6, 2\n"
#define WRITE_DCCSW(rs)  "mcr p15, 0, " #rs ", c7, c10, 2\n"

/**
 * First macro to do a set/way operation on all entries of all data caches
 *
 * Must be inserted directly before the targeted operation. Returns operand
 * for targeted operation in R6.
 */
#define FOR_ALL_SET_WAY_OF_ALL_DATA_CACHES_0 \
 \
	/* get the cache level value (Clidr::Loc) */ \
	READ_CLIDR(r0) \
	"ands r3, r0, #0x7000000\n" \
	"mov r3, r3, lsr #23\n" \
 \
	/* skip all if cache level value is zero */ \
	"beq 5f\n" \
	"mov r9, #0\n" \
 \
	/* begin loop over cache numbers */ \
	"1:\n" \
 \
	/* work out 3 x cache level */ \
	"add r2, r9, r9, lsr #1\n" \
 \
	/* get the cache type of current cache number (Clidr::CtypeX) */ \
	"mov r1, r0, lsr r2\n" \
	"and r1, r1, #7\n" \
	"cmp r1, #2\n" \
 \
	/* skip cache number if there's no data cache at this level */ \
	"blt 4f\n" \
 \
	/* select the appropriate CCSIDR according to cache level and type */ \
	WRITE_CSSELR(r9) \
	"isb\n" \
 \
	/* get the line length of current cache (Ccsidr::LineSize) */ \
	READ_CCSIDR(r1) \
	"and r2, r1, #0x7\n" \
 \
	/* add 4 for the line-length offset (log2 of 16 bytes) */ \
	"add r2, r2, #4\n" \
 \
	/* get the associativity or max way size (Ccsidr::Associativity) */ \
	"ldr r4, =0x3ff\n" \
	"ands r4, r4, r1, lsr #3\n" \
 \
	/* get the bit position of the way-size increment */ \
	"clz r5, r4\n" \
 \
	/* get a working copy of the max way size */ \
	"mov r8, r4\n" \
 \
	/* begin loop over way numbers */ \
	"2:\n" \
 \
	/* get the number of sets or the max index size (Ccsidr::NumSets) */ \
	"ldr r7, =0x00007fff\n" \
	"ands r7, r7, r1, lsr #13\n" \
 \
	/* begin loop over indices */ \
	"3:\n" \
 \
	/* factor in the way number and cache number into write value */ \
	"orr r6, r9, r8, lsl r5\n" \
 \
	/* factor in the index number into write value */ \
	"orr r6, r6, r7, lsl r2\n"

/**
 * Second macro to do a set/way operation on all entries of all data caches
 *
 * Must be inserted directly after the targeted operation.
 */
#define FOR_ALL_SET_WAY_OF_ALL_DATA_CACHES_1 \
 \
		/* decrement the index */ \
		"subs r7, r7, #1\n" \
 \
		/* end loop over indices */ \
		"bge 3b\n" \
 \
		/* decrement the way number */ \
		"subs r8, r8, #1\n" \
 \
		/* end loop over way numbers */ \
		"bge 2b\n" \
 \
		/* label to skip a cache number */ \
		"4:\n" \
 \
		/* increment the cache number */ \
		"add r9, r9, #2\n" \
		"cmp r3, r9\n" \
 \
		/* end loop over cache numbers */ \
		"bgt 1b\n" \
 \
		/* synchronize data */ \
		"dsb\n" \
 \
		/* label to skip all */ \
		"5:\n" \
		::: "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9"

namespace Arm_v7
{
	using namespace Genode;

	/**
	 * CPU driver for core
	 */
	struct Processor_driver : Arm::Processor_driver
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
		struct Sctlr : Arm::Processor_driver::Sctlr
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
				       Arm::Processor_driver::Sctlr::init_phys_kernel() |
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
				       Arm::Processor_driver::Sctlr::init_virt_kernel() |
				       Sw::bits(0) |
				       Ha::bits(0) |
				       Nmfi::bits(0) |
				       Tre::bits(0);
			}
		};

		/**
		 * Translation table base register 0
		 */
		struct Ttbr0 : Arm::Processor_driver::Ttbr0
		{
			struct Nos : Bitfield<5,1> { }; /* not outer shareable */

			struct Irgn_1 : Bitfield<0,1> { }; /* inner cachable mode */
			struct Irgn_0 : Bitfield<6,1> { }; /* inner cachable mode */

			/**
			 * Value for the switch to virtual mode in kernel
			 *
			 * \param sect_table  pointer to initial section table
			 */
			static access_t init_virt_kernel(addr_t const sect_table)
			{
				return Arm::Processor_driver::Ttbr0::init_virt_kernel(sect_table) |
				       Nos::bits(0) |
				       Irgn_1::bits(0) |
				       Irgn_0::bits(1);
			}
		};

		/**
		 * Translation table base control register
		 */
		struct Ttbcr : Arm::Processor_driver::Ttbcr
		{
			struct Pd0 : Bitfield<4,1> { }; /* disable walk for TTBR0 */
			struct Pd1 : Bitfield<5,1> { }; /* disable walk for TTBR1 */

			/**
			 * Value for the switch to virtual mode in kernel
			 */
			static access_t init_virt_kernel()
			{
				return Arm::Processor_driver::Ttbcr::init_virt_kernel() |
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
		                             unsigned const process_id)
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
			Board::prepare_kernel();
			Sctlr::write(Sctlr::init_phys_kernel());
			Psr::write(Psr::init_kernel());
			flush_tlb();
		}

		/**
		 * Wether we are in secure mode
		 */
		static bool secure_mode()
		{
			if (!Board::SECURITY_EXTENSION) return 0;
			return !Scr::Ns::get(Scr::read());
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

		/**
		 * Invalidate all predictions about the future control-flow
		 */
		static void invalidate_control_flow_predictions()
		{
			asm volatile ("mcr p15, 0, r0, c7, c5, 6");
		}

		/**
		 * Finish all previous data transfers
		 */
		static void data_synchronization_barrier() { asm volatile ("dsb"); }

		/**
		 * Enable secondary processors that loop on wait-for-event
		 *
		 * \param ip  initial instruction pointer for secondary processors
		 */
		static void start_secondary_processors(void * const ip)
		{
			if (PROCESSORS > 1) {
				Genode::Board::secondary_processors_ip(ip);
				data_synchronization_barrier();
				asm volatile ("sev\n");
			}
		}

		/**
		 * Wait for the next interrupt as cheap as possible
		 */
		static void wait_for_interrupt() { asm volatile ("wfi"); }
	};
}


/***************************
 ** Arm::Processor_driver **
 ***************************/

void Arm::Processor_driver::flush_data_caches()
{
	asm volatile (
		FOR_ALL_SET_WAY_OF_ALL_DATA_CACHES_0
		WRITE_DCCSW(r6)
		FOR_ALL_SET_WAY_OF_ALL_DATA_CACHES_1);
}


void Arm::Processor_driver::invalidate_data_caches()
{
	asm volatile (
		FOR_ALL_SET_WAY_OF_ALL_DATA_CACHES_0
		WRITE_DCISW(r6)
		FOR_ALL_SET_WAY_OF_ALL_DATA_CACHES_1);
}


Arm::Processor_driver::Psr::access_t Arm::Processor_driver::Psr::init_user_with_trustzone()
{
	return M::bits(M::USER) |
	       T::bits(T::ARM) |
	       F::bits(0) |
	       I::bits(1) |
	       A::bits(1) |
	       E::bits(E::LITTLE) |
	       J::bits(J::ARM);
}


#endif /* _PROCESSOR_DRIVER__ARM_V7_H_ */

