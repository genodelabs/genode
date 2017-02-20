/*
 * \brief  Board driver for core on pandaboard
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2014-06-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC__PANDA__BOARD_H_
#define _CORE__INCLUDE__SPEC__PANDA__BOARD_H_

/* core includes */
#include <spec/arm/pl310.h>
#include <spec/cortex_a9/board_support.h>

namespace Genode { class Board; }


class Genode::Board : public Cortex_a9::Board
{
	public:

		using Base = Cortex_a9::Board;

		/**
		 * Frontend to firmware running in the secure world
		 */
		struct Secure_monitor
		{
			enum Syscalls
			{
				CPU_ACTLR_SMP_BIT_RAISE =  0x25,
				L2_CACHE_SET_DEBUG_REG  = 0x100,
				L2_CACHE_ENABLE_REG     = 0x102,
				L2_CACHE_AUX_REG        = 0x109,
			};

			void call(addr_t func, addr_t val)
			{
				register addr_t _func asm("r12") = func;
				register addr_t _val  asm("r0")  = val;
				asm volatile("dsb; smc #0" ::
				             "r" (_func), "r" (_val) :
				             "memory", "cc", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
				             "r8", "r9", "r10", "r11");
			}
		};


		class L2_cache : public Base::L2_cache
		{
			private:

				Secure_monitor _monitor;

				unsigned long _init_value()
				{
					Aux::access_t v = 0;
					Aux::Associativity::set(v, 1);
					Aux::Way_size::set(v, 3);
					Aux::Share_override::set(v, 1);
					Aux::Reserved::set(v, 1);
					Aux::Ns_lockdown::set(v, 1);
					Aux::Ns_irq_ctrl::set(v, 1);
					Aux::Data_prefetch::set(v, 1);
					Aux::Inst_prefetch::set(v, 1);
					Aux::Early_bresp::set(v, 1);
					return v;
				}

				unsigned long _debug_value()
				{
					Debug::access_t v = 0;
					Debug::Dwb::set(v, 1);
					Debug::Dcl::set(v, 1);
					return v;
				}

			public:

				L2_cache(Genode::addr_t mmio) : Base::L2_cache(mmio)
				{
					_monitor.call(Secure_monitor::L2_CACHE_AUX_REG,
					              _init_value());
				}

				void clean_invalidate()
				{
					_monitor.call(Secure_monitor::L2_CACHE_SET_DEBUG_REG,
					              _debug_value());
					Base::L2_cache::clean_invalidate();
					_monitor.call(Secure_monitor::L2_CACHE_SET_DEBUG_REG, 0);
				}

				void invalidate() { Base::L2_cache::invalidate(); }

				void enable()
				{
					_monitor.call(Secure_monitor::L2_CACHE_ENABLE_REG, 1);
					Base::L2_cache::mask_interrupts();
				}

				void disable() {
					_monitor.call(Secure_monitor::L2_CACHE_ENABLE_REG, 0); }
		};

		L2_cache & l2_cache() { return _l2_cache; }

	private:

		L2_cache _l2_cache { Base::l2_cache().base };
};

#endif /* _CORE__INCLUDE__SPEC__PANDA__BOARD_H_ */
