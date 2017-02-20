/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__CORTEX_A15__CPU_H_
#define _CORE__INCLUDE__SPEC__CORTEX_A15__CPU_H_

/* core includes */
#include <translation_table.h>
#include <spec/arm_v7/cpu_support.h>

namespace Genode { class Cpu; }


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

		/**
		 * Memory attribute indirection register 0
		 */
		struct Mair0 : Arm_v7::Mair0
		{
			enum Attr {
				DEVICE_MEMORY          = 0x04,
				NORMAL_MEMORY_UNCACHED = 0x44,
				NORMAL_MEMORY_CACHED   = 0xff,
			};

			static access_t init_virt_kernel()
			{
				access_t v = 0;
				Attr0::set(v, NORMAL_MEMORY_UNCACHED);
				Attr1::set(v, DEVICE_MEMORY);
				Attr2::set(v, NORMAL_MEMORY_CACHED);
				Attr3::set(v, DEVICE_MEMORY);
				return v;
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


		/*********************************
		 **  Virtualization extensions  **
		 *********************************/

		/**
		 * Hypervisor translation table base register
		 */
		struct Httbr : Register<64>
		{
			static void translation_table(addr_t const table)
			{
				asm volatile ("mcrr p15, 4, %[v0], %[v1], c2"
				              :: [v0]"r"(table), [v1]"r"(0));
			}
		};

		/**
		 * Hypervisor translation control register
		 */
		struct Htcr : Register<32>
		{
			static void write(access_t const v) {
				asm volatile ("mcr p15, 4, %[v], c2, c0, 2" :: [v] "r" (v)); }
		};

		/**
		 * Hypervisor coprocessor trap register
		 */
		struct Hcptr : Register<32>
		{
			/* Coprocessor access trap */
			template <unsigned COPROC>
			struct Tcp : Bitfield<COPROC, 1> {};

			struct Tase  : Bitfield<15, 1> { };
			struct Tta   : Bitfield<20, 1> { };
			struct Tcpac : Bitfield<31, 1> { };

			static access_t init()
			{
				/* don't trap on cporocessor 10 + 11, but all others */
				access_t v = 0;
				Tcp<0>::set(v, 1);
				Tcp<1>::set(v, 1);
				Tcp<2>::set(v, 1);
				Tcp<3>::set(v, 1);
				Tcp<4>::set(v, 1);
				Tcp<5>::set(v, 1);
				Tcp<6>::set(v, 1);
				Tcp<7>::set(v, 1);
				Tcp<8>::set(v, 1);
				Tcp<9>::set(v, 1);
				Tcp<12>::set(v, 1);
				Tcp<13>::set(v, 1);
				Tta::set(v, 1);
				Tcpac::set(v, 1);
				return v;
			}

			static void write(access_t const v) {
				asm volatile ("mcr p15, 4, %[v], c1, c1, 2" :: [v] "r" (v)); }
		};

		/**
		 * Hypervisor Memory attribute indirection register 0
		 */
		struct Hmair0 : Register<32>
		{
			static void write(access_t const v) {
				asm volatile ("mcr p15, 4, %[v], c10, c2, 0" :: [v] "r" (v)); }
		};

		/**
		 * Hypervisor system control register
		 */
		struct Hsctlr : Arm_v7::Sctlr
		{
			static void write(access_t const v) {
				asm volatile ("mcr p15, 4, %[v], c1, c0, 0" :: [v] "r" (v)); }
		};

		/**
		 * Hypervisor system trap register
		 */
		struct Hstr : Register<32>
		{
			/* System coprocessor primary register access trap */
			template <unsigned R>
			struct T : Bitfield<R, 1> {};

			static access_t init()
			{
				/*
				 * allow cache (7), TLB (8) maintenance, and performance
				 * monitor (9), process/thread ID register (13) and timer (14)
				 * access.
				 */
				access_t v = 0;
				T<0>::set(v, 1);
				T<1>::set(v, 1);
				T<2>::set(v, 1);
				T<3>::set(v, 1);
				T<5>::set(v, 1);
				T<6>::set(v, 1);
				T<10>::set(v, 1);
				T<11>::set(v, 1);
				T<12>::set(v, 1);
				T<15>::set(v, 1);
				return v;
			};
		};

		/**
		 * Hypervisor control register
		 */
		struct Hcr : Register<32>
		{
			struct Vm    : Bitfield<0, 1>  {}; /* VT MMU enabled            */
			struct Fmo   : Bitfield<3, 1>  {}; /* FIQ cannot been masked    */
			struct Imo   : Bitfield<4, 1>  {}; /* IRQ cannot been masked    */
			struct Amo   : Bitfield<5, 1>  {}; /* A bit cannot been masked  */
			struct Twi   : Bitfield<13, 1> {}; /* trap on WFI instruction   */
			struct Twe   : Bitfield<14, 1> {}; /* trap on WFE instruction   */
			struct Tidcp : Bitfield<20, 1> {}; /* trap lockdown             */
			struct Tac   : Bitfield<21, 1> {}; /* trap ACTLR accesses       */
			struct Tvm   : Bitfield<26, 1> {}; /* trap virtual memory ctrls */

			static access_t init()
			{
				access_t v = 0;
				Vm::set(v, 1);
				Fmo::set(v, 1);
				Imo::set(v, 1);
				Amo::set(v, 1);
				Twi::set(v, 1);
				Twe::set(v, 1);
				Tidcp::set(v, 1);
				Tac::set(v, 1);
				Tvm::set(v, 1);
				return v;
			};
		};

		/**
		 * Virtualization translation control register
		 */
		struct Vtcr : Ttbcr
		{
			struct Sl0 : Bitfield<6,2> {};

			static access_t init()
			{
				access_t v = Ttbcr::init_virt_kernel();
				Sl0::set(v, 1); /* set to starting level 1 */
				return v;
			}

			static void write(access_t const v) {
				asm volatile ("mcr p15, 4, %[v], c2, c1, 2" :: [v] "r" (v)); }
		};

		/**
		 * Extend basic CPU state by members relevant for 'base-hw' only
		 *
		 * Note: this class redefines Genode::Arm::Context
		 */
		struct Context : Genode::Cpu_state
		{
			Ttbr0::access_t ttbr0 = 0;
			Sctlr::access_t sctlr = 0;
			Ttbcr::access_t ttbrc = 0;
			Mair0::access_t mair0 = 0;

			/**
			 * Return base of assigned translation table
			 */
			addr_t translation_table() const {
				return Ttbr0::Ba::masked(ttbr0); }

			/**
			 * Assign translation-table base 'table'
			 */
			void translation_table(addr_t const table) {
				Ttbr0::Ba::set(ttbr0, (Ttbr0::access_t)(table >> 5)); }

			/**
			 * Assign protection domain
			 */
			void protection_domain(Genode::uint8_t const id) {
				Ttbr0::Asid::set(ttbr0, id); }
		};


		/**
		 * An usermode execution state
		 *
		 * FIXME: this class largely overlaps with Genode::Arm::User_context
		 */
		struct User_context : Context
		{
			/**
			 * Constructor
			 */
			User_context() { cpsr = Psr::init_user(); }

			/**
			 * Support for kernel calls
			 */
			void user_arg_0(unsigned const arg) { r0 = arg; }
			void user_arg_1(unsigned const arg) { r1 = arg; }
			void user_arg_2(unsigned const arg) { r2 = arg; }
			void user_arg_3(unsigned const arg) { r3 = arg; }
			void user_arg_4(unsigned const arg) { r4 = arg; }
			void user_arg_5(unsigned const arg) { r5 = arg; }
			void user_arg_6(unsigned const arg) { r6 = arg; }
			void user_arg_7(unsigned const arg) { r7 = arg; }
			unsigned user_arg_0() const { return r0; }
			unsigned user_arg_1() const { return r1; }
			unsigned user_arg_2() const { return r2; }
			unsigned user_arg_3() const { return r3; }
			unsigned user_arg_4() const { return r4; }
			unsigned user_arg_5() const { return r5; }
			unsigned user_arg_6() const { return r6; }
			unsigned user_arg_7() const { return r7; }

			/**
			 * Initialize thread context
			 *
			 * \param table  physical base of appropriate translation table
			 * \param pd_id  kernel name of appropriate protection domain
			 */
			void init_thread(addr_t const table, unsigned const pd_id)
			{
				protection_domain(pd_id);
				translation_table(table);
			}

			/**
			 * Return if the context is in a page fault due to translation miss
			 *
			 * \param va  holds the virtual fault-address if call returns 1
			 * \param w   holds wether it's a write fault if call returns 1
			 */
			bool in_fault(addr_t & va, addr_t & w) const
			{
				switch (cpu_exception) {

				case PREFETCH_ABORT:
					{
						/* check if fault was caused by a translation miss */
						Ifsr::access_t const fs = Ifsr::Fs::get(Ifsr::read());
						if ((fs & 0b11100) != 0b100) return false;

						/* fetch fault data */
						w = 0;
						va = ip;
						return true;
					}

				case DATA_ABORT:
					{
						/* check if fault was caused by translation miss */
						Dfsr::access_t const fs = Dfsr::Fs::get(Dfsr::read());
						if ((fs & 0b11100) != 0b100) return false;

						/* fetch fault data */
						Dfsr::access_t const dfsr = Dfsr::read();
						w = Dfsr::Wnr::get(dfsr);
						va = Dfar::read();
						return true;
					}

				default:
					return false;
				};
			}
		};


		/**
		 * Return kernel name of the executing CPU
		 */
		static unsigned executing_id() { return Mpidr::Aff_0::get(Mpidr::read()); }

		/**
		 * Return kernel name of the primary CPU
		 */
		static unsigned primary_id();

		/**
		 * Switch to the virtual mode in kernel
		 *
		 * \param pd  kernel pd object pointer
		 */
		static void init_virt_kernel(Kernel::Pd & pd);

		/**
		 * Write back dirty cache lines and invalidate all cache lines
		 */
		void clean_invalidate_data_cache() {
			clean_invalidate_inner_data_cache(); }

		/**
		 * Invalidate all cache lines
		 */
		void invalidate_data_cache() {
			invalidate_inner_data_cache(); }

		void translation_table_insertions() { invalidate_branch_predicts(); }

		/**
		 * Hook function called at the very beginning
		 * of the local cpu initialization
		 */
		void init(Genode::Translation_table&);

		/**
		 * Switch on MMU and caches
		 *
		 * \param table  physical page table address
		 */
		void enable_mmu_and_caches(Genode::addr_t table)
		{
			Cpu::Mair0::write(Cpu::Mair0::init_virt_kernel());
			Cpu::Dacr::write(Cpu::Dacr::init_virt_kernel());
			Cpu::Ttbr0::write(Cpu::Ttbr0::init(table, 0));
			Cpu::Ttbcr::write(Cpu::Ttbcr::init_virt_kernel());
			Cpu::Sctlr::enable_mmu_and_caches();
			invalidate_branch_predicts();
		}


		/*************
		 ** Dummies **
		 *************/

		void switch_to(User_context&) { }
		bool retry_undefined_instr(Context&) { return false; }
};

#endif /* _CPU_H_ */
