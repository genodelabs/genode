/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2012-09-11
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROCESSOR_DRIVER__ARM_H_
#define _PROCESSOR_DRIVER__ARM_H_

/* Genode includes */
#include <util/register.h>
#include <cpu/cpu_state.h>

/* local includes */
#include <board.h>
#include <util.h>

namespace Arm
{
	using namespace Genode;

	/**
	 * CPU driver for core
	 */
	struct Processor_driver
	{
		enum {
			TTBCR_N = 0,
			EXCEPTION_ENTRY = 0xffff0000,
			DATA_ACCESS_ALIGNM = 4,
		};

		/**
		 * Multiprocessor affinity register
		 */
		struct Mpidr : Register<32>
		{
			struct Aff_0 : Bitfield<0, 8> { };

			/**
			 * Read register value
			 */
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %[v], c0, c0, 5" : [v] "=r" (v) ::);
				return v;
			}
		};

		/**
		 * Cache type register
		 */
		struct Ctr : Register<32>
		{
			/**
			 * Read register value
			 */
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %[v], c0, c0, 1" : [v]"=r"(v) :: );
				return v;
			}
		};

		/**
		 * System control register
		 */
		struct Sctlr : Register<32>
		{
			struct M : Bitfield<0,1> { };  /* enable MMU */
			struct A : Bitfield<1,1> { };  /* strict data addr. alignment on */
			struct C : Bitfield<2,1> { };  /* enable data cache */
			struct Z : Bitfield<11,1> { }; /* enable program flow prediction */
			struct I : Bitfield<12,1> { }; /* enable instruction caches */

			/*
			 * These must be set all ones
			 */
			struct Static1 : Bitfield<3,4> { };
			struct Static2 : Bitfield<16,1> { };
			struct Static3 : Bitfield<18,1> { };
			struct Static4 : Bitfield<22,2> { };

			struct V : Bitfield<13,1> /* select exception-entry base */
			{
				enum { XFFFF0000 = 1 };
			};

			struct Rr : Bitfield<14,1> /* replacement strategy */
			{
				enum { RANDOM = 0 };
			};

			struct Fi : Bitfield<21,1> { }; /* enable fast IRQ config */

			struct Ve : Bitfield<24,1> /* interrupt vector config */
			{
				enum { FIXED = 0 };
			};

			struct Ee : Bitfield<25,1> { }; /* raise CPSR.E on exceptions */

			/**
			 * Common bitfield values for all modes
			 */
			static access_t common()
			{
				return Static1::bits(~0) |
				       Static2::bits(~0) |
				       Static3::bits(~0) |
				       Static4::bits(~0) |
				       A::bits(0) |
				       C::bits(1) |
				       Z::bits(0) |
				       I::bits(1) |
				       V::bits(V::XFFFF0000) |
				       Rr::bits(Rr::RANDOM) |
				       Fi::bits(0) |
				       Ve::bits(Ve::FIXED) |
				       Ee::bits(0);
			}

			/**
			 * Value for the switch to virtual mode in kernel
			 */
			static access_t init_virt_kernel() {
				return common() | M::bits(1); }

			/**
			 * Value for the initial kernel entry
			 */
			static access_t init_phys_kernel() {
				return common() | M::bits(0); }

			/**
			 * Read register value
			 */
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %[v], c1, c0, 0" : [v]"=r"(v) :: );
				return v;
			}

			/**
			 * Write register value
			 */
			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %[v], c1, c0, 0" :: [v]"r"(v) : ); }
		};

		/**
		 * Translation table base control register
		 */
		struct Ttbcr : Register<32>
		{
			struct N : Bitfield<0, 3> { }; /* base address width */

			/**
			 * Write register, only in privileged CPU mode
			 */
			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %[v], c2, c0, 2" :: [v]"r"(v) : ); }

			/**
			 * Read register value
			 */
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %[v], c2, c0, 2" : [v]"=r"(v) :: );
				return v;
			}

			/**
			 * Value for the switch to virtual mode in kernel
			 */
			static access_t init_virt_kernel() { return N::bits(TTBCR_N); }
		};

		/**
		 * Translation table base register 0
		 */
		struct Ttbr0 : Register<32>
		{
			struct S : Bitfield<1,1> { }; /* shareable */

			struct Rgn : Bitfield<3, 2> /* outer cachable attributes */
			{
				enum { NON_CACHEABLE = 0 };
			};

			struct Ba : Bitfield<14-TTBCR_N, 18+TTBCR_N> { }; /* translation
			                                                   * table base */

			/**
			 * Write register, only in privileged CPU mode
			 */
			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %[v], c2, c0, 0" :: [v]"r"(v) : ); }

			/**
			 * Read register value
			 */
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %[v], c2, c0, 0" : [v]"=r"(v) :: );
				return v;
			}

			/**
			 * Value for the switch to virtual mode in kernel
			 *
			 * \param sect_table  pointer to initial section table
			 */
			static access_t init_virt_kernel(addr_t const sect_table)
			{
				return S::bits(0) |
				       Rgn::bits(Rgn::NON_CACHEABLE) |
				       Ba::masked((addr_t)sect_table);
			}
		};

		/**
		 * Domain access control register
		 */
		struct Dacr : Register<32>
		{
			enum Dx_values { NO_ACCESS = 0, CLIENT = 1 };

			/**
			 * Access values for the 16 available domains
			 */
			struct D0  : Bitfield<0,2> { };
			struct D1  : Bitfield<2,2> { };
			struct D2  : Bitfield<4,2> { };
			struct D3  : Bitfield<6,2> { };
			struct D4  : Bitfield<8,2> { };
			struct D5  : Bitfield<10,2> { };
			struct D6  : Bitfield<12,2> { };
			struct D7  : Bitfield<14,2> { };
			struct D8  : Bitfield<16,2> { };
			struct D9  : Bitfield<18,2> { };
			struct D10 : Bitfield<20,2> { };
			struct D11 : Bitfield<22,2> { };
			struct D12 : Bitfield<24,2> { };
			struct D13 : Bitfield<26,2> { };
			struct D14 : Bitfield<28,2> { };
			struct D15 : Bitfield<30,2> { };

			/**
			 * Write register, only in privileged CPU mode
			 */
			static void write(access_t const v) {
				asm volatile ("mcr p15, 0, %[v], c3, c0, 0" :: [v]"r"(v) : ); }

			/**
			 * Initialize for Genodes operational mode
			 */
			static access_t init_virt_kernel()
			{
				return D0::bits(CLIENT)     | D1::bits(NO_ACCESS)  |
				       D2::bits(NO_ACCESS)  | D3::bits(NO_ACCESS)  |
				       D4::bits(NO_ACCESS)  | D5::bits(NO_ACCESS)  |
				       D6::bits(NO_ACCESS)  | D7::bits(NO_ACCESS)  |
				       D8::bits(NO_ACCESS)  | D9::bits(NO_ACCESS)  |
				       D10::bits(NO_ACCESS) | D11::bits(NO_ACCESS) |
				       D12::bits(NO_ACCESS) | D13::bits(NO_ACCESS) |
				       D14::bits(NO_ACCESS) | D15::bits(NO_ACCESS);
			}
		};

		/**
		 * Instruction Cache Invalidate by MVA to PoU
		 */
		struct Icimvau : Register<32>
		{
			/**
			 * Write register value
			 */
			static void write(access_t const v)
			{
				asm volatile (
					"mcr p15, 0, %[v], c7, c5, 1\n" :: [v] "r" (v) : );
			}
		};

		/**
		 * Data Cache Clean by MVA to PoC
		 */
		struct Dccmvac : Register<32>
		{
			/**
			 * Write register value
			 */
			static void write(access_t const v)
			{
				asm volatile (
					"mcr p15, 0, %[v], c7, c10, 1\n" :: [v] "r" (v) : );
			}
		};

		/**
		 * Context identification register
		 */
		struct Cidr : Register<32>
		{
			/**
			 * Write register value
			 */
			static void write(access_t const v)
			{
				asm volatile ("mcr p15, 0, %[v], c13, c0, 1" :: [v]"r"(v) : );
			}

			/**
			 * Read register value
			 */
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %[v], c13, c0, 1" : [v]"=r"(v) :: );
				return v;
			}
		};

		/**
		 * Program status register
		 */
		struct Psr : Register<32>
		{
			struct M : Bitfield<0,5> /* processor mode */
			{
				enum { USER = 0b10000, SUPERVISOR = 0b10011 };
			};

			struct T : Bitfield<5,1> /* instruction state */
			{
				enum { ARM = 0 };
			};

			struct F : Bitfield<6,1> { }; /* FIQ disable */
			struct I : Bitfield<7,1> { }; /* IRQ disable */
			struct A : Bitfield<8,1> { }; /* asynchronous abort disable */

			struct E : Bitfield<9,1> /* load/store endianess */
			{
				enum { LITTLE = 0 };
			};

			struct J : Bitfield<24,1> /* instruction state */
			{
				enum { ARM = 0 };
			};

			/**
			 * Read register
			 */
			static access_t read()
			{
				access_t v;
				asm volatile ("mrs %[v], cpsr" : [v] "=r" (v) : : );
				return v;
			}

			/**
			 * Write register
			 */
			static void write(access_t const v) {
				asm volatile ("msr cpsr, %[v]" : : [v] "r" (v) : ); }

			/**
			 * Initial value for a user execution context with trustzone
			 *
			 * FIXME: This function should not be declared in 'Arm' but in
			 *        'Arm_v7', but for now the declaration is necessary
			 *        because of 'User_context::User_context()'.
			 */
			inline static access_t init_user_with_trustzone();

			/**
			 * Initial value for an userland execution context
			 */
			static access_t init_user()
			{
				return M::bits(M::USER) |
				       T::bits(T::ARM) |
				       F::bits(1) |
				       I::bits(0) |
				       A::bits(1) |
				       E::bits(E::LITTLE) |
				       J::bits(J::ARM);
			}

			/**
			 * Initial value for the kernel execution context
			 */
			static access_t init_kernel()
			{
				return M::bits(M::SUPERVISOR) |
				       T::bits(T::ARM) |
				       F::bits(1) |
				       I::bits(1) |
				       A::bits(1) |
				       E::bits(E::LITTLE) |
				       J::bits(J::ARM);
			}
		};

		/**
		 * Common parts of fault status registers
		 */
		struct Fsr : Register<32>
		{
			/**
			 * Fault status encoding
			 */
			enum Fault_status
			{
				SECTION_TRANSLATION = 5,
				PAGE_TRANSLATION = 7,
			};

			struct Fs_3_0 : Bitfield<0, 4> { };  /* fault status */
			struct Fs_4   : Bitfield<10, 1> { }; /* fault status */
		};

		/**
		 * Instruction fault status register
		 */
		struct Ifsr : Fsr
		{
			/**
			 * Read register value
			 */
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %[v], c5, c0, 1" : [v]"=r"(v) :: );
				return v;
			}

			/**
			 * Read fault status
			 */
			static Fault_status fault_status()
			{
				access_t const v = read();
				return (Fault_status)(Fs_3_0::get(v) |
				                     (Fs_4::get(v) << Fs_3_0::WIDTH));
			}
		};

		/**
		 * Data fault status register
		 */
		struct Dfsr : Fsr
		{
			struct Wnr : Bitfield<11, 1> { }; /* write not read bit */

			/**
			 * Read register value
			 */
			static access_t read()
			{
				access_t v;
				asm volatile ("mrc p15, 0, %[v], c5, c0, 0" : [v]"=r"(v) :: );
				return v;
			}

			/**
			 * Read fault status
			 */
			static Fault_status fault_status() {
				access_t const v = read();
				return (Fault_status)(Fs_3_0::get(v) |
				                     (Fs_4::get(v) << Fs_3_0::WIDTH));
			}
		};

		/**
		 * Data fault address register
		 */
		struct Dfar : Register<32>
		{
			/**
			 * Read register value
			 */
			static access_t read() {
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
			/**********************************************************
			 ** The offset and width of any of these classmembers is **
			 ** silently expected to be this way by several assembly **
			 ** files. So take care if you attempt to change them.   **
			 **********************************************************/

			uint32_t cidr;          /* context ID register backup */
			uint32_t section_table; /* base address of applied section table */

			/**
			 * Get base of assigned translation lookaside buffer
			 */
			addr_t tlb() const { return section_table; }

			/**
			 * Assign translation lookaside buffer
			 */
			void tlb(addr_t const st) { section_table = st; }

			/**
			 * Assign protection domain
			 */
			void protection_domain(unsigned const id) { cidr = id; }
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

			/***************************************************
			 ** Communication between user and context holder **
			 ***************************************************/

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
			 * \param tlb    physical base of appropriate page table
			 * \param pd_id  kernel name of appropriate protection domain
			 */
			void init_thread(addr_t const tlb, unsigned const pd_id)
			{
				cidr = pd_id;
				section_table = tlb;
			}

			/**
			 * Return if the context is in a page fault due to a translation miss
			 *
			 * \param va  holds the virtual fault-address if call returns 1
			 * \param w   holds wether it's a write fault if call returns 1
			 */
			bool in_fault(addr_t & va, addr_t & w) const
			{
				/* determine fault type */
				switch (cpu_exception) {

				case PREFETCH_ABORT: {

					/* check if fault was caused by a translation miss */
					Ifsr::Fault_status const fs = Ifsr::fault_status();
					if (fs == Ifsr::SECTION_TRANSLATION ||
					    fs == Ifsr::PAGE_TRANSLATION)
					{
						/* fetch fault data */
						w = 0;
						va = ip;
						return 1;
					}
					return 0; }

				case DATA_ABORT: {

					/* check if fault was caused by translation miss */
					Dfsr::Fault_status const fs = Dfsr::fault_status();
					if(fs == Dfsr::SECTION_TRANSLATION ||
					   fs == Dfsr::PAGE_TRANSLATION)
					{
						/* fetch fault data */
						Dfsr::access_t const dfsr = Dfsr::read();
						w = Dfsr::Wnr::get(dfsr);
						va = Dfar::read();
						return 1;
					}
					return 0; }

				default: return 0;
				}
			}
		};

		/**
		 * Invalidate all entries of all instruction caches
		 */
		__attribute__((always_inline))
		static void invalidate_instruction_caches()
		{
			asm volatile ("mcr p15, 0, %[rd], c7, c5, 0" :: [rd]"r"(0) : );
		}

		/**
		 * Flush all entries of all data caches
		 */
		inline static void flush_data_caches();

		/**
		 * Invalidate all entries of all data caches
		 */
		inline static void invalidate_data_caches();

		/**
		 * Flush all caches
		 */
		static void flush_caches()
		{
			flush_data_caches();
			invalidate_instruction_caches();
		}

		/**
		 * Invalidate all TLB entries of one address space
		 *
		 * \param pid  ID of the targeted address space
		 */
		static void flush_tlb_by_pid(unsigned const pid)
		{
			asm volatile ("mcr p15, 0, %[pid], c8, c7, 2" :: [pid]"r"(pid) : );
			flush_caches();
		}

		/**
		 * Invalidate all TLB entries
		 */
		static void flush_tlb()
		{
			asm volatile ("mcr p15, 0, %[rd], c8, c7, 0" :: [rd]"r"(0) : );
			flush_caches();
		}

		/**
		 * Clean every data-cache entry within a virtual region
		 */
		static void
		flush_data_caches_by_virt_region(addr_t base, size_t const size)
		{
			enum {
				LINE_SIZE        = 1 << Board::CACHE_LINE_SIZE_LOG2,
				LINE_ALIGNM_MASK = ~(LINE_SIZE - 1),
			};
			addr_t const top = base + size;
			base = base & LINE_ALIGNM_MASK;
			for (; base < top; base += LINE_SIZE) { Dccmvac::write(base); }
		}

		/**
		 * Invalidate every instruction-cache entry within a virtual region
		 */
		static void
		invalidate_instr_caches_by_virt_region(addr_t base, size_t const size)
		{
			enum {
				LINE_SIZE        = 1 << Board::CACHE_LINE_SIZE_LOG2,
				LINE_ALIGNM_MASK = ~(LINE_SIZE - 1),
			};
			addr_t const top = base + size;
			base = base & LINE_ALIGNM_MASK;
			for (; base < top; base += LINE_SIZE) { Icimvau::write(base); }
		}
	};
}

#endif /* _PROCESSOR_DRIVER__ARM_H_ */

