/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2012-09-11
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SPEC__ARM__CPU_SUPPORT_H_
#define _SPEC__ARM__CPU_SUPPORT_H_

/* Genode includes */
#include <util/register.h>
#include <cpu/cpu_state.h>

/* local includes */
#include <board.h>
#include <util.h>

namespace Genode
{
	/**
	 * CPU driver for core
	 */
	class Arm;

	typedef Genode::uint64_t sizet_arithm_t;
}

class Genode::Arm
{
	public:

		static constexpr addr_t exception_entry   = 0xffff0000;
		static constexpr addr_t mtc_size          = 1 << MIN_PAGE_SIZE_LOG2;
		static constexpr addr_t data_access_align = 4;

		/**
		 * Multiprocessor affinity register
		 */
		struct Mpidr : Register<32>
		{
			struct Aff_0 : Bitfield<0, 8> { }; /* affinity value 0 */

			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %0, c0, c0, 5" : "=r" (v) :: );
				return v;
			}
		};

		/**
		 * Cache type register
		 */
		struct Ctr : Register<32>
		{
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %0, c0, c0, 1" : "=r" (v) :: );
				return v;
			}
		};

		/**
		 * System control register
		 */
		struct Sctlr : Register<32>
		{
			struct M : Bitfield<0,1>  { }; /* enable MMU */
			struct C : Bitfield<2,1>  { }; /* enable data cache */
			struct I : Bitfield<12,1> { }; /* enable instruction caches */
			struct V : Bitfield<13,1> { }; /* select exception entry */

			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %0, c1, c0, 0" : "=r" (v) :: );
				return v;
			}

			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %0, c1, c0, 0" :: "r" (v) : ); }

			/**
			 * Do initialization that is common on value 'v'
			 */
			static void init_common(access_t & v)
			{
				C::set(v, 1);
				I::set(v, 1);
				V::set(v, 1);
			}

			/**
			 * Do initialization for virtual mode in kernel on value 'v'
			 */
			static void init_virt_kernel(access_t & v) { M::set(v, 1); }
		};

		/**
		 * Translation table base control register
		 */
		struct Ttbcr : Register<32>
		{
			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %0, c2, c0, 2" :: "r" (v) : ); }

			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %0, c2, c0, 2" : "=r" (v) :: );
				return v;
			}
		};

		/**
		 * Translation table base register 0
		 */
		struct Ttbr0 : Register<32>
		{
			enum Memory_region { NON_CACHEABLE = 0, CACHEABLE = 1 };

			struct C   : Bitfield<0,1> { };    /* inner cacheable */
			struct S   : Bitfield<1,1> { };    /* shareable */
			struct Rgn : Bitfield<3,2> { };    /* outer cachable mode */
			struct Nos : Bitfield<5,1> { };    /* not outer shareable */
			struct Ba  : Bitfield<14, 18> { }; /* translation table base */


			/*************************************
			 *  with multiprocessing extension  **
			 *************************************/

			struct Irgn_1 : Bitfield<0,1> { };
			struct Irgn_0 : Bitfield<6,1> { };
			struct Irgn   : Bitset_2<Irgn_0, Irgn_1> { }; /* inner cache mode */

			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %0, c2, c0, 0" :: "r" (v) : ); }

			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r" (v) :: );
				return v;
			}

			/**
			 * Return initialized value
			 *
			 * \param table  base of targeted translation table
			 */
			static access_t init(addr_t const table)
			{
				access_t v = Ba::masked((addr_t)table);
				Rgn::set(v, CACHEABLE);
				S::set(v, Board::is_smp() ? 1 : 0);
				if (Board::is_smp()) Irgn::set(v, CACHEABLE);
				else C::set(v, 1);
				return v;
			}
		};

		/**
		 * Domain access control register
		 */
		struct Dacr : Register<32>
		{
			struct D0 : Bitfield<0,2> { }; /* access mode for domain 0 */

			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %0, c3, c0, 0" :: "r" (v) : ); }

			/**
			 * Return value initialized for virtual mode in kernel
			 */
			static access_t init_virt_kernel() { return D0::bits(1); }
		};

		/**
		 * Instruction Cache Invalidate by MVA to PoU
		 */
		struct Icimvau : Register<32>
		{
			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %0, c7, c5, 1" :: "r" (v) : ); }
		};

		/**
		 * Data Cache Clean and Invalidate by MVA to PoC
		 */
		struct Dccimvac : Register<32>
		{
			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %0, c7, c14, 1" :: "r" (v) : ); }
		};

		/**
		 * Context identification register
		 */
		struct Cidr : Register<32>
		{
			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %0, c13, c0, 1" :: "r" (v) : ); }

			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %0, c13, c0, 1" : "=r" (v) :: );
				return v;
			}
		};

		/**
		 * Program status register
		 */
		struct Psr : Register<32>
		{
			/**
			 * CPU mode
			 */
			struct M : Bitfield<0,5>
			{
				enum {
					USR = 16,
					SVC = 19,
					MON = 22,
					HYP = 26,
				};
			};

			struct F : Bitfield<6,1> { }; /* FIQ disable */
			struct I : Bitfield<7,1> { }; /* IRQ disable */
			struct A : Bitfield<8,1> { }; /* async. abort disable */

			static access_t read()
			{
				access_t v;
				asm volatile ("mrs %0, cpsr" : "=r" (v) :: );
				return v;
			}

			static void write(access_t const v) {
				asm volatile ("msr cpsr, %0" :: "r" (v) : ); }

			/**
			 * Return value initialized for user execution with trustzone
			 */
			static access_t init_user_with_trustzone();

			/**
			 * Do common initialization on register value 'v'
			 */
			static void init_common(access_t & v)
			{
				F::set(v, 1);
				A::set(v, 1);
			}

			/**
			 * Return initial value for user execution
			 */
			static access_t init_user()
			{
				access_t v = 0;
				init_common(v);
				M::set(v, M::USR);
				return v;
			}

			/**
			 * Return initial value for the kernel
			 */
			static access_t init_kernel()
			{
				access_t v = 0;
				init_common(v);
				M::set(v, M::SVC);
				I::set(v, 1);
				return v;
			}
		};

		/**
		 * Common parts of fault status registers
		 */
		struct Fsr : Register<32>
		{
			static constexpr access_t section = 5;
			static constexpr access_t page    = 7;

			struct Fs_0 : Bitfield<0, 4> { };       /* fault status */
			struct Fs_1 : Bitfield<10, 1> { };      /* fault status */
			struct Fs   : Bitset_2<Fs_0, Fs_1> { }; /* fault status */
		};

		/**
		 * Instruction fault status register
		 */
		struct Ifsr : Fsr
		{
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %0, c5, c0, 1" : "=r" (v) :: );
				return v;
			}
		};

		/**
		 * Data fault status register
		 */
		struct Dfsr : Fsr
		{
			struct Wnr : Bitfield<11, 1> { }; /* write not read bit */

			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %0, c5, c0, 0" : "=r" (v) :: );
				return v;
			}
		};

		/**
		 * Data fault address register
		 */
		struct Dfar : Register<32>
		{
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %[v], c6, c0, 0" : [v]"=r"(v) :: );
				return v;
			}
		};

		/**
		 * Extend basic CPU state by members relevant for 'base-hw' only
		 */
		struct Context : Cpu_state
		{
			Cidr::access_t  cidr;
			Ttbr0::access_t ttbr0;

			/**
			 * Return base of assigned translation table
			 */
			addr_t translation_table() const {
				return Ttbr0::Ba::masked(ttbr0); }


			/**
			 * Assign translation-table base 'table'
			 */
			void translation_table(addr_t const table) {
				ttbr0 = Arm::Ttbr0::init(table); }

			/**
			 * Assign protection domain
			 */
			void protection_domain(Genode::uint8_t const id) { cidr = id; }
		};

		/**
		 * This class comprises ARM specific protection domain attributes
		 */
		struct Pd
		{
			Genode::uint8_t asid; /* address space id */

			Pd(Genode::uint8_t id) : asid(id) {}
		};

		/**
		 * An usermode execution state
		 */
		struct User_context : Context
		{
			/**
			 * Constructor
			 */
			User_context();

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
						if (fs != Ifsr::section && fs != Ifsr::page)
							return false;

						/* fetch fault data */
						w = 0;
						va = ip;
						return true;
					}
				case DATA_ABORT:
					{
						/* check if fault was caused by translation miss */
						Dfsr::access_t const fs = Dfsr::Fs::get(Dfsr::read());
						if (fs != Dfsr::section && fs != Dfsr::page)
							return false;

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
		 * Returns true if current execution context is running in user mode
		 */
		static bool is_user() { return Psr::M::get(Psr::read()) == Psr::M::USR; }

		/**
		 * Invalidate all entries of all instruction caches
		 */
		__attribute__((always_inline)) static void invalidate_instr_caches() {
			asm volatile ("mcr p15, 0, %0, c7, c5, 0" :: "r" (0) : ); }

		/**
		 * Flush all entries of all data caches
		 */
		static void flush_data_caches();

		/**
		 * Invalidate all entries of all data caches
		 */
		static void invalidate_data_caches();

		/**
		 * Flush all caches
		 */
		static void flush_caches()
		{
			flush_data_caches();
			invalidate_instr_caches();
		}

		/**
		 * Invalidate all TLB entries of the address space named 'pid'
		 */
		static void flush_tlb_by_pid(unsigned const pid)
		{
			flush_caches();
			asm volatile ("mcr p15, 0, %0, c8, c7, 2" :: "r" (pid) : );
		}

		/**
		 * Invalidate all TLB entries
		 */
		static void flush_tlb()
		{
			flush_caches();
			asm volatile ("mcr p15, 0, %0, c8, c7, 0" :: "r" (0) : );
		}

		static constexpr addr_t line_size = 1 << Board::CACHE_LINE_SIZE_LOG2;
		static constexpr addr_t line_align_mask = ~(line_size - 1);

		/**
		 * Flush data-cache entries for virtual region ['base', 'base + size')
		 */
		static void
		flush_data_caches_by_virt_region(addr_t base, size_t const size)
		{
			addr_t const top = base + size;
			base &= line_align_mask;
			for (; base < top; base += line_size) { Dccimvac::write(base); }
		}

		/**
		 * Bin instr.-cache entries for virtual region ['base', 'base + size')
		 */
		static void
		invalidate_instr_caches_by_virt_region(addr_t base, size_t const size)
		{
			addr_t const top = base + size;
			base &= line_align_mask;
			for (; base < top; base += line_size) { Icimvau::write(base); }
		}
};

#endif /* _SPEC__ARM__CPU_SUPPORT_H_ */
