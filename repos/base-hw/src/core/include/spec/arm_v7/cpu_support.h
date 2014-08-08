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

#ifndef _SPEC__ARM_V7__CPU_SUPPORT_H_
#define _SPEC__ARM_V7__CPU_SUPPORT_H_

/* core includes */
#include <spec/arm/cpu_support.h>
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

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Arm_v7;
}

class Genode::Arm_v7 : public Arm
{
	protected:

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
			struct Cpnsae10 : Bitfield<10, 1> { };
			struct Cpnsae11 : Bitfield<11, 1> { };
		};

		/**
		 * System control register
		 */
		struct Sctlr : Arm::Sctlr
		{
			struct Z : Bitfield<11,1> { }; /* enable program flow prediction */
			struct Unnamed_0 : Bitfield<3,4>  { }; /* shall be ones */
			struct Unnamed_1 : Bitfield<16,1> { }; /* shall be ones */
			struct Unnamed_2 : Bitfield<18,1> { }; /* shall be ones */
			struct Unnamed_3 : Bitfield<22,2> { }; /* shall be ones */

			/**
			 * Initialization that is common
			 */
			static void init_common(access_t & v)
			{
				Arm::Sctlr::init_common(v);
				Unnamed_0::set(v, ~0);
				Unnamed_1::set(v, ~0);
				Unnamed_2::set(v, ~0);
				Unnamed_3::set(v, ~0);
			}

			/**
			 * Initialization for virtual kernel stage
			 */
			static access_t init_virt_kernel()
			{
				access_t v = 0;
				init_common(v);
				Arm::Sctlr::init_virt_kernel(v);
				Z::set(v, 1);
				return v;
			}

			/**
			 * Initialization for physical kernel stage
			 */
			static access_t init_phys_kernel()
			{
				access_t v = 0;
				init_common(v);
				return v;
			}
		};

	public:

		/**
		 * Translation table base register 0
		 */
		struct Ttbr0 : Arm::Ttbr0
		{
			struct Irgn_1 : Bitfield<0, 1> { }; /* inner cache attr */
			struct Rgn    : Bitfield<3, 2> { }; /* outer cache attr */
			struct Irgn_0 : Bitfield<6, 1> { }; /* inner cache attr */
			struct Irgn   : Bitset_2<Irgn_0, Irgn_1> { }; /* inner cache attr */

			/**
			 * Return value initialized with translation table 'table'
			 */
			static access_t init(addr_t const table)
			{
				access_t v = Ba::masked(table);
				Irgn::set(v, 1);
				Rgn::set(v, 1);
				return v;
			}
		};

		/**
		 * Invalidate all branch predictions
		 */
		static void inval_branch_predicts() {
			asm volatile ("mcr p15, 0, r0, c7, c5, 6" ::: "r0"); };

		/**
		 * Switch to the virtual mode in kernel
		 *
		 * \param table       base of targeted translation table
		 * \param process_id  process ID of the kernel address-space
		 */
		static void
		init_virt_kernel(addr_t const table, unsigned const process_id)
		{
			Cidr::write(process_id);
			Dacr::write(Dacr::init_virt_kernel());
			Ttbr0::write(Ttbr0::init(table));
			Ttbcr::write(0);
			Sctlr::write(Sctlr::init_virt_kernel());
			inval_branch_predicts();
		}

		inline static void finish_init_phys_kernel();

		/**
		 * Configure this module appropriately for the first kernel run
		 */
		static void init_phys_kernel()
		{
			Board::prepare_kernel();
			Sctlr::write(Sctlr::init_phys_kernel());
			Psr::write(Psr::init_kernel());
			flush_tlb();
			finish_init_phys_kernel();
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
		 * Set exception-vector's address for monitor mode to 'a'
		 */
		static void mon_exception_entry_at(addr_t const a) {
			asm volatile ("mcr p15, 0, %[rd], c12, c0, 1" : : [rd] "r" (a)); }

		/**
		 * Enable access of co-processors cp10 and cp11 from non-secure mode.
		 */
		static inline void allow_coprocessor_nonsecure()
		{
			Nsacr::access_t v = 0;
			Nsacr::Cpnsae10::set(v, 1);
			Nsacr::Cpnsae11::set(v, 1);
			asm volatile ("mcr p15, 0, %[v], c1, c1, 2" : : [v] "r" (v));
		}

		/**
		 * Finish all previous data transfers
		 */
		static void data_synchronization_barrier() { asm volatile ("dsb"); }

		/**
		 * Enable secondary processors with instr. pointer 'ip'
		 */
		static void start_secondary_processors(void * const ip)
		{
			if (!is_smp()) { return; }
			Board::secondary_processors_ip(ip);
			data_synchronization_barrier();
			asm volatile ("sev\n");
		}

		/**
		 * Wait for the next interrupt as cheap as possible
		 */
		static void wait_for_interrupt() { asm volatile ("wfi"); }
};


void Genode::Arm::flush_data_caches()
{
	asm volatile (
		FOR_ALL_SET_WAY_OF_ALL_DATA_CACHES_0
		WRITE_DCCSW(r6)
		FOR_ALL_SET_WAY_OF_ALL_DATA_CACHES_1);
	Board::outer_cache_flush();
}


void Genode::Arm::invalidate_data_caches()
{
	asm volatile (
		FOR_ALL_SET_WAY_OF_ALL_DATA_CACHES_0
		WRITE_DCISW(r6)
		FOR_ALL_SET_WAY_OF_ALL_DATA_CACHES_1);
	Board::outer_cache_invalidate();
}


Genode::Arm::Psr::access_t Genode::Arm::Psr::init_user_with_trustzone()
{
	access_t v = 0;
	M::set(v, usr);
	I::set(v, 1);
	A::set(v, 1);
	return v;
}

#endif /* _SPEC__ARM_V7__CPU_SUPPORT_H_ */
