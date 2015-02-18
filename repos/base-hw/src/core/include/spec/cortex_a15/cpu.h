/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CPU_H_
#define _CPU_H_

/* core includes */
#include <spec/arm_v7/cpu_support.h>

namespace Genode
{
	/**
	 * Part of CPU state that is not switched on every mode transition
	 */
	class Cpu_lazy_state { };

	/**
	 * CPU driver for core
	 */
	class Cpu;
}

namespace Kernel { using Genode::Cpu_lazy_state; }

class Genode::Cpu : public Arm_v7
{
	public:

		/**
		 * Translation table base control register
		 */
		struct Ttbcr : Arm::Ttbcr
		{
			struct Irgn0 : Bitfield<8,  2> { };
			struct Orgn0 : Bitfield<10, 2> { };
			struct Sh0   : Bitfield<12, 2> { };
			struct Eae   : Bitfield<31, 1> { }; /* extended address enable */

			static access_t init_virt_kernel()
			{
				access_t v = 0;
				Irgn0::set(v, 1);
				Orgn0::set(v, 1);
				Sh0::set(v, 0b10);
				Eae::set(v, 1);
				return v;
			}
		};

		struct Mair0 : Register<32>
		{
			static void init()
			{
				access_t v = 0xff0044;
				asm volatile ("mcr p15, 0, %[v], c10, c2, 0" :: [v]"r"(v) : );
			}
		};

		/**
		 * Non-secure access control register
		 */
		struct Nsacr : Arm_v7::Nsacr
		{
			struct Ns_smp : Bitfield<18,1> { };
		};

		/**
		 * Translation table base register 0 (64-bit format)
		 */
		struct Ttbr0 : Register<64>
		{
			enum Memory_region { NON_CACHEABLE = 0, CACHEABLE = 1 };

			struct Ba   : Bitfield<5, 34> { }; /* translation table base */
			struct Asid : Bitfield<48,8>  { };

			static void write(access_t const v)
			{
				asm volatile ("mcrr p15, 0, %[v0], %[v1], c2"
				              :: [v0]"r"(v), [v1]"r"(v >> 32) : );
			}

			static access_t read()
			{
				uint32_t v0, v1;
				asm volatile ("mrrc p15, 0, %[v0], %[v1], c2"
				              : [v0]"=r"(v0), [v1]"=r"(v1) :: );
				return (access_t) v0 | ((access_t)v1 << 32);
			}

			/**
			 * Return initialized value
			 *
			 * \param table  base of targeted translation table
			 */
			static access_t init(addr_t const table, unsigned const id)
			{
				access_t v = Ba::masked((access_t)table);
				Asid::set(v, id);
				return v;
			}

			static Genode::uint32_t init(addr_t const table) {
				return table; }

		};


		/**
		 * Return wether to retry an undefined user instruction after this call
		 */
		bool retry_undefined_instr(Cpu_lazy_state *) { return false; }

		/**
		 * Return kernel name of the executing CPU
		 */
		static unsigned executing_id();

		/**
		 * Return kernel name of the primary CPU
		 */
		static unsigned primary_id();

		/**
		 * Switch to the virtual mode in kernel
		 *
		 * \param table       base of targeted translation table
		 * \param process_id  process ID of the kernel address-space
		 */
		static void
		init_virt_kernel(addr_t const table, unsigned const process_id)
		{
			Mair0::init();
			Cidr::write(process_id);
			Dacr::write(Dacr::init_virt_kernel());
			Ttbr0::write(Ttbr0::init(table, 1));
			Ttbcr::write(Ttbcr::init_virt_kernel());
			Sctlr::write(Sctlr::init_virt_kernel());
			inval_branch_predicts();
		}

		/*************
		 ** Dummies **
		 *************/

		static void tlb_insertions() { inval_branch_predicts(); }
		static void translation_added(addr_t, size_t) { }
		static void prepare_proceeding(Cpu_lazy_state *, Cpu_lazy_state *) { }
};

void Genode::Arm_v7::finish_init_phys_kernel() { }

#endif /* _CPU_H_ */
