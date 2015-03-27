/*
 * \brief  Board driver for core on pandaboard
 * \author Stefan Kalkowski
 * \date   2014-06-02
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

/* core includes */
#include <util/mmio.h>
#include <spec/cortex_a9/board_support.h>

namespace Genode
{
	struct Board : Cortex_a9::Board
	{
		/**
		 * L2 outer cache controller
		 */
		struct Pl310 : Mmio {

			enum Trustzone_hypervisor_syscalls {
				L2_CACHE_SET_DEBUG_REG = 0x100,
				L2_CACHE_ENABLE_REG    = 0x102,
				L2_CACHE_AUX_REG       = 0x109,
			};

			static inline void
			trustzone_hypervisor_call(addr_t func, addr_t val)
			{
				register addr_t _func asm("r12") = func;
				register addr_t _val  asm("r0")  = val;
				asm volatile("dsb; smc #0" :: "r" (_func), "r" (_val)
				             : "memory", "cc", "r1", "r2", "r3", "r4", "r5",
				               "r6", "r7", "r8", "r9", "r10", "r11");
			}

			struct Control   : Register <0x100, 32>
			{
				struct Enable : Bitfield<0,1> {};
			};

			struct Aux : Register<0x104, 32>
			{
				struct Associativity  : Bitfield<16,1> { };
				struct Way_size       : Bitfield<17,3> { };
				struct Share_override : Bitfield<22,1> { };
				struct Reserved       : Bitfield<25,1> { };
				struct Ns_lockdown    : Bitfield<26,1> { };
				struct Ns_irq_ctrl    : Bitfield<27,1> { };
				struct Data_prefetch  : Bitfield<28,1> { };
				struct Inst_prefetch  : Bitfield<29,1> { };
				struct Early_bresp    : Bitfield<30,1> { };

				static access_t init_value()
				{
					access_t v = 0;
					Associativity::set(v, 1);
					Way_size::set(v, 3);
					Share_override::set(v, 1);
					Reserved::set(v, 1);
					Ns_lockdown::set(v, 1);
					Ns_irq_ctrl::set(v, 1);
					Data_prefetch::set(v, 1);
					Inst_prefetch::set(v, 1);
					Early_bresp::set(v, 1);
					return v;
				}
			};

			struct Irq_mask                : Register <0x214, 32> {};
			struct Irq_clear               : Register <0x220, 32> {};
			struct Cache_sync              : Register <0x730, 32> {};
			struct Invalidate_by_way       : Register <0x77c, 32> {};
			struct Clean_invalidate_by_way : Register <0x7fc, 32> {};

			inline void sync() { while (read<Cache_sync>()) ; }

			void invalidate()
			{
				write<Invalidate_by_way>((1 << 16) - 1);
				sync();
			}

			void flush()
			{
				trustzone_hypervisor_call(L2_CACHE_SET_DEBUG_REG, 0x3);
				write<Clean_invalidate_by_way>((1 << 16) - 1);
				sync();
				trustzone_hypervisor_call(L2_CACHE_SET_DEBUG_REG, 0x0);
			}

			Pl310(addr_t const base) : Mmio(base)
			{
				trustzone_hypervisor_call(L2_CACHE_AUX_REG,
				                          Pl310::Aux::init_value());
				trustzone_hypervisor_call(L2_CACHE_ENABLE_REG, 1);
				write<Irq_mask>(0);
				write<Irq_clear>(0xffffffff);
			}
		};

		static void outer_cache_invalidate();
		static void outer_cache_flush();
		static void prepare_kernel();
		static void secondary_cpus_ip(void * const ip) { }
		static bool is_smp() { return true; }
	};
}

#endif /* _BOARD_H_ */
