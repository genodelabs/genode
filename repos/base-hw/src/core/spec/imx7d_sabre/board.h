/*
 * \brief  Board driver for core
 * \author Stefan Kalkowski
 * \date   2018-11-07
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__IMX7D_SABRE__BOARD_H_
#define _CORE__SPEC__IMX7D_SABRE__BOARD_H_

#include <hw/spec/arm/imx7d_sabre_board.h>
#include <spec/arm/virtualization/gicv2.h>
#include <spec/arm/generic_timer.h>
#include <spec/arm/cpu/vm_state_virtualization.h>
#include <translation_table.h>
#include <kernel/configuration.h>

namespace Kernel { class Cpu; }

namespace Board {
	using namespace Hw::Imx7d_sabre_board;

	struct Virtual_local_pic {};

	enum { TIMER_IRQ = 30, VCPU_MAX = 1 };

	using Vm_state = Genode::Vm_state;
	using Vm_page_table = Hw::Level_1_stage_2_translation_table;
	using Vm_page_table_array =
		Vm_page_table::Allocator::Array<Kernel::DEFAULT_TRANSLATION_TABLE_MAX>;

	struct Vcpu_context { Vcpu_context(Kernel::Cpu &) {} };
}

#endif /* _CORE__SPEC__IMX7_SABRELITE__BOARD_H_ */
