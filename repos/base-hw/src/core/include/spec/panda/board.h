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
	class L2_cache;
	class Board;
}

/**
 * L2 outer cache controller
 */
class Genode::L2_cache
{
	private:

		struct Secure_monitor
		{
			enum Syscalls
			{
				L2_CACHE_SET_DEBUG_REG = 0x100,
				L2_CACHE_ENABLE_REG    = 0x102,
				L2_CACHE_AUX_REG       = 0x109,
			};

			void call(addr_t func, addr_t val)
			{
				register addr_t _func asm("r12") = func;
				register addr_t _val  asm("r0")  = val;
				asm volatile(
					"dsb; smc #0" ::
					"r" (_func), "r" (_val) :
					"memory", "cc", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
					"r8", "r9", "r10", "r11");
			}
		} _secure_monitor;

		Arm::Pl310 _pl310;

	public:

		L2_cache() : _pl310(Cortex_a9::Board::PL310_MMIO_BASE)
		{
			Arm::Pl310::Aux::access_t v = 0;
			Arm::Pl310::Aux::Associativity::set(v, 1);
			Arm::Pl310::Aux::Way_size::set(v, 3);
			Arm::Pl310::Aux::Share_override::set(v, 1);
			Arm::Pl310::Aux::Reserved::set(v, 1);
			Arm::Pl310::Aux::Ns_lockdown::set(v, 1);
			Arm::Pl310::Aux::Ns_irq_ctrl::set(v, 1);
			Arm::Pl310::Aux::Data_prefetch::set(v, 1);
			Arm::Pl310::Aux::Inst_prefetch::set(v, 1);
			Arm::Pl310::Aux::Early_bresp::set(v, 1);

			_secure_monitor.call(Secure_monitor::L2_CACHE_AUX_REG, v);
			_secure_monitor.call(Secure_monitor::L2_CACHE_ENABLE_REG, 1);
			_pl310.mask_interrupts();
		}

		void flush()
		{
			_secure_monitor.call(Secure_monitor::L2_CACHE_SET_DEBUG_REG, 0x3);
			_pl310.flush();
			_secure_monitor.call(Secure_monitor::L2_CACHE_SET_DEBUG_REG, 0x0);
		}

		void invalidate() { _pl310.invalidate(); }
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
