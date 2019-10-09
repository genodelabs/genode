/*
 * \brief   Pandaboard specific definitions
 * \author  Stefan Kalkowski
 * \date    2017-02-20
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__PANDA__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__PANDA__BOARD_H_

#include <hw/spec/arm/panda_board.h>
#include <spec/arm/cortex_a9_page_table.h>
#include <spec/arm/cpu.h>
#include <hw/spec/arm/gicv2.h>

namespace Board {
	using namespace Hw::Panda_board;
	using Pic = Hw::Gicv2;
	static constexpr bool NON_SECURE = false;

	class L2_cache;
}

namespace Bootstrap { struct Actlr; }


struct Bootstrap::Actlr
{
	static void enable_smp()
	{
		using namespace Board;
		call_panda_firmware(CPU_ACTLR_SMP_BIT_RAISE, 0);
	}

	static void disable_smp() { /* not implemented */ }
};


class Board::L2_cache : Hw::Pl310
{
	private:

		unsigned long _init_value()
		{
			Aux::access_t v = 0;
			Aux::Associativity::set(v, Aux::Associativity::WAY_16);
			Aux::Way_size::set(v, Aux::Way_size::KB_64);
			Aux::Share_override::set(v, true);
			Aux::Replacement_policy::set(v, Aux::Replacement_policy::PRAND);
			Aux::Ns_lockdown::set(v,   true);
			Aux::Ns_irq_ctrl::set(v,   true);
			Aux::Data_prefetch::set(v, true);
			Aux::Inst_prefetch::set(v, true);
			Aux::Early_bresp::set(v,   true);
			return v;
		}

	public:

		L2_cache(Genode::addr_t mmio) : Hw::Pl310(mmio) {
			call_panda_firmware(L2_CACHE_AUX_REG, _init_value()); }

		using Hw::Pl310::invalidate;

		void enable()
		{
			call_panda_firmware(L2_CACHE_ENABLE_REG, 1);
			Pl310::mask_interrupts();
		}

		void disable() {
			call_panda_firmware(L2_CACHE_ENABLE_REG, 0); }
};

#endif /* _SRC__BOOTSTRAP__SPEC__PANDA__BOARD_H_ */
