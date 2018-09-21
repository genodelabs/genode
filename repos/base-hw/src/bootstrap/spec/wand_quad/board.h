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

#ifndef _SRC__BOOTSTRAP__SPEC__WAND_QUAD__BOARD_H_
#define _SRC__BOOTSTRAP__SPEC__WAND_QUAD__BOARD_H_

#include <drivers/defs/wand_quad.h>
#include <drivers/uart/imx.h>
#include <hw/spec/arm/cortex_a9.h>
#include <hw/spec/arm/pl310.h>

#include <spec/arm/cortex_a9_actlr.h>
#include <spec/arm/cortex_a9_page_table.h>
#include <spec/arm/cpu.h>
#include <spec/arm/pic.h>

namespace Board {

	using namespace Wand_quad;

	struct L2_cache;
	using Cpu_mmio = Hw::Cortex_a9_mmio<CORTEX_A9_PRIVATE_MEM_BASE>;
	using Serial   = Genode::Imx_uart;

	enum {
		UART_BASE  = UART_1_MMIO_BASE,
		UART_CLOCK = 0, /* dummy value, not used */
	};
}

struct Board::L2_cache : Hw::Pl310
{
	L2_cache(Genode::addr_t mmio) : Hw::Pl310(mmio)
	{
		Aux::access_t aux = 0;
		Aux::Full_line_of_zero::set(aux, true);
		Aux::Associativity::set(aux, Aux::Associativity::WAY_16);
		Aux::Way_size::set(aux, Aux::Way_size::KB_64);
		Aux::Share_override::set(aux, true);
		Aux::Replacement_policy::set(aux, Aux::Replacement_policy::PRAND);
		Aux::Ns_lockdown::set(aux, true);
		Aux::Data_prefetch::set(aux, true);
		Aux::Inst_prefetch::set(aux, true);
		Aux::Early_bresp::set(aux, true);
		write<Aux>(aux);

		Tag_ram::access_t tag_ram = 0;
		Tag_ram::Setup_latency::set(tag_ram, 2);
		Tag_ram::Read_latency::set(tag_ram, 3);
		Tag_ram::Write_latency::set(tag_ram, 1);
		write<Tag_ram>(tag_ram);

		Data_ram::access_t data_ram = 0;
		Data_ram::Setup_latency::set(data_ram, 2);
		Data_ram::Read_latency::set(data_ram, 3);
		Data_ram::Write_latency::set(data_ram, 1);
		write<Data_ram>(data_ram);

		Prefetch_ctrl::access_t prefetch = 0;
		Prefetch_ctrl::Data_prefetch::set(prefetch, 1);
		Prefetch_ctrl::Inst_prefetch::set(prefetch, 1);
		write<Prefetch_ctrl>(prefetch | 0xF);
	}

	using Hw::Pl310::invalidate;

	void enable()
	{
		Pl310::mask_interrupts();
		write<Control::Enable>(1);
	}

	void disable() {
		write<Control::Enable>(0);
	}
};

#endif /* _SRC__BOOTSTRAP__SPEC__WAND_QUAD__BOARD_H_ */
