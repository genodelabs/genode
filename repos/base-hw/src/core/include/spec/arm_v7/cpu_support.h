/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__ARM_V7__CPU_SUPPORT_H_
#define _CORE__INCLUDE__SPEC__ARM_V7__CPU_SUPPORT_H_

/* core includes */
#include <spec/arm/cpu_support.h>
#include <board.h>
#include <pic.h>

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Arm_v7;
}


class Genode::Arm_v7 : public Arm
{
	public:

		/**
		 * Secure configuration register
		 */
		struct Scr : Register<32>
		{
			struct Ns  : Bitfield<0, 1> { }; /* not secure               */
			struct Fw  : Bitfield<4, 1> { }; /* F bit writeable          */
			struct Aw  : Bitfield<5, 1> { }; /* A bit writeable          */
			struct Scd : Bitfield<7, 1> { }; /* smc call disable         */
			struct Hce : Bitfield<8, 1> { }; /* hyp call enable          */
			struct Sif : Bitfield<9, 1> { }; /* secure instruction fetch */

			/**
			 * Read register value
			 */
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %[v], c1, c1, 0" : [v]"=r"(v) ::);
				return v;
			}

			/**
			 * Write register value
			 */
			static void write(access_t const v)
			{
				asm volatile ("mcr p15, 0, %[v], c1, c1, 0 \n"
				              "isb" : : [v] "r" (v));
			}
		};

		/**
		 * Non-secure access control register
		 */
		struct Nsacr : Register<32>
		{
			struct Cpnsae10 : Bitfield<10, 1> { };
			struct Cpnsae11 : Bitfield<11, 1> { };

			/**
			 * Write register value
			 */
			static void write(access_t const v)
			{
				asm volatile ("mcr p15, 0, %[v], c1, c1, 2" : : [v] "r" (v));
			}
		};

		/**
		 * System control register
		 */
		struct Sctlr : Arm::Sctlr
		{
			struct Z : Bitfield<11,1> { }; /* enable program flow prediction */

			static access_t init_value()
			{
				access_t v = read();
				C::set(v, 1);
				I::set(v, 1);
				V::set(v, 1);
				A::set(v, 0);
				M::set(v, 1);
				Z::set(v, 1);
				return v;
			}

			static void enable_mmu_and_caches() {
				write(init_value()); }
		};


		/**
		 * Memory attribute indirection register 0
		 */
		struct Mair0 : Register<32>
		{
			struct Attr0 : Bitfield<0,  8> { };
			struct Attr1 : Bitfield<8,  8> { };
			struct Attr2 : Bitfield<16, 8> { };
			struct Attr3 : Bitfield<24, 8> { };

			static void write(access_t v) {
				asm volatile ("mcr p15, 0, %[v], c10, c2, 0" :: [v]"r"(v) : ); }
		};

		/**
		 * Finish all previous data transfers
		 */
		static void data_synchronization_barrier() { asm volatile ("dsb"); }

		/**
		 * Wait for the next interrupt as cheap as possible
		 */
		static void wait_for_interrupt() { asm volatile ("wfi"); }

		/**
		 * Write back dirty lines of inner data cache and invalidate all
		 */
		void clean_invalidate_inner_data_cache();

		/**
		 * Invalidate all lines of the inner data cache
		 */
		void invalidate_inner_data_cache();

		/**
		 * Invalidate all lines of the instruction cache
		 */
		void invalidate_instruction_cache() {
			asm volatile("mcr p15, 0, r0, c7, c5, 0"); }


		/******************************
		 **  Trustzone specific API  **
		 ******************************/

		/**
		 * Wether we are in secure mode
		 */
		static bool secure_mode()
		{
			if (!Board::SECURITY_EXTENSION) return 0;
			return !Scr::Ns::get(Scr::read());
		}

		/**
		 * Set exception-vector's address for monitor mode to 'a'
		 */
		static void mon_exception_entry_at(addr_t const a) {
			asm volatile ("mcr p15, 0, %[rd], c12, c0, 1" : : [rd] "r" (a)); }


		/***********************************
		 **  Virtualization specific API  **
		 ***********************************/

		/**
		 * Set exception-vector's address for hypervisor mode to 'a'
		 */
		static void hyp_exception_entry_at(void * a) {
			asm volatile ("mcr p15, 4, %[rd], c12, c0, 0" :: [rd] "r" (a)); }
};

#endif /* _CORE__INCLUDE__SPEC__ARM_V7__CPU_SUPPORT_H_ */
