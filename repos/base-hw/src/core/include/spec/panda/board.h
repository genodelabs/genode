/*
 * \brief  Board driver for core on pandaboard
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2014-06-02
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BOARD_H_
#define _BOARD_H_

/* core includes */
#include <spec/cortex_a9/board_support.h>
#include <spec/arm/pl310.h>

namespace Genode
{
	class Pl310;
	class Board;
}

/**
 * L2 outer cache controller
 */
class Genode::Pl310 : public Arm::Pl310
{
	private:

		enum Secure_monitor_syscalls
		{
			L2_CACHE_SET_DEBUG_REG = 0x100,
			L2_CACHE_ENABLE_REG    = 0x102,
			L2_CACHE_AUX_REG       = 0x109,
		};

		static inline void _secure_monitor_call(addr_t func, addr_t val)
		{
			register addr_t _func asm("r12") = func;
			register addr_t _val  asm("r0")  = val;
			asm volatile(
				"dsb; smc #0" ::
				"r" (_func), "r" (_val) :
				"memory", "cc", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
				"r8", "r9", "r10", "r11");
		}

	public:

		void flush()
		{
			_secure_monitor_call(L2_CACHE_SET_DEBUG_REG, 0x3);
			Arm::Pl310::flush();
			_secure_monitor_call(L2_CACHE_SET_DEBUG_REG, 0x0);
		}

		Pl310(addr_t const base) : Arm::Pl310(base)
		{
			_secure_monitor_call(L2_CACHE_AUX_REG, Aux::init_value());
			_secure_monitor_call(L2_CACHE_ENABLE_REG, 1);
			_init();
		}
};

/**
 * Board driver for core
 */
class Genode::Board : public Cortex_a9::Board
{
	public:

		static void outer_cache_invalidate();
		static void outer_cache_flush();
		static void prepare_kernel();
		static void secondary_cpus_ip(void * const ip) { }
		static bool is_smp() { return true; }
};

#endif /* _BOARD_H_ */
