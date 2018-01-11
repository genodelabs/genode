/*
 * \brief   Pbxa9 specific board definitions
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

#include <drivers/defs/panda.h>
#include <drivers/uart/tl16c750.h>
#include <hw/spec/arm/cortex_a9.h>
#include <hw/spec/arm/pl310.h>
#include <hw/spec/arm/panda_trustzone_firmware.h>

#include <spec/arm/cortex_a9_page_table.h>
#include <spec/arm/cpu.h>
#include <spec/arm/pic.h>

namespace Board {
	class L2_cache;

	using namespace Panda;
	using Cpu_mmio = Hw::Cortex_a9_mmio<CORTEX_A9_PRIVATE_MEM_BASE>;
	using Serial   = Genode::Tl16c750_uart;

	enum {
		UART_BASE  = TL16C750_3_MMIO_BASE,
		UART_CLOCK = TL16C750_CLOCK,
	};
}

namespace Bootstrap { struct Actlr; }


struct Bootstrap::Actlr
{
	static void enable_smp() {
		Hw::call_panda_firmware(Hw::CPU_ACTLR_SMP_BIT_RAISE, 0); }

	static void disable_smp() { /* not implemented */ }
};


class Board::L2_cache : Hw::Pl310
{
	private:

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

	public:

		L2_cache(Genode::addr_t mmio) : Hw::Pl310(mmio) {
			Hw::call_panda_firmware(Hw::L2_CACHE_AUX_REG, _init_value()); }

		using Hw::Pl310::invalidate;

		void enable()
		{
			Hw::call_panda_firmware(Hw::L2_CACHE_ENABLE_REG, 1);
			Pl310::mask_interrupts();
		}

		void disable() {
			Hw::call_panda_firmware(Hw::L2_CACHE_ENABLE_REG, 0); }
};

#endif /* _SRC__BOOTSTRAP__SPEC__PANDA__BOARD_H_ */
