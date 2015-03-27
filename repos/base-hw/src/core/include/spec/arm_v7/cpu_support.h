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

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Arm_v7;
}

namespace Kernel { class Pd; }


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
		 * Invalidate all branch predictions
		 */
		static void inval_branch_predicts() {
			asm volatile ("mcr p15, 0, r0, c7, c5, 6" ::: "r0"); };

		/**
		 * Switch to the virtual mode in kernel
		 *
		 * \param pd  kernel's pd object
		 */
		static void init_virt_kernel(Kernel::Pd* pd);

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
		 * Finish all previous data transfers
		 */
		static void data_synchronization_barrier() { asm volatile ("dsb"); }

		/**
		 * Enable secondary CPUs with instr. pointer 'ip'
		 */
		static void start_secondary_cpus(void * const ip)
		{
			if (!(NR_OF_CPUS > 1)) { return; }
			Board::secondary_cpus_ip(ip);
			data_synchronization_barrier();
			asm volatile ("sev\n");
		}

		/**
		 * Wait for the next interrupt as cheap as possible
		 */
		static void wait_for_interrupt() { asm volatile ("wfi"); }


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

#endif /* _SPEC__ARM_V7__CPU_SUPPORT_H_ */
