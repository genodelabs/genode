/*
 * \brief  Board driver for core
 * \author Stefan Kalkowski
 * \date   2017-04-27
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARNDALE__BOARD_H_
#define _CORE__SPEC__ARNDALE__BOARD_H_

#include <hw/spec/arm/gicv2.h>
#include <hw/spec/arm/arndale_board.h>
#include <spec/arm/exynos_mct.h>
#include <spec/arm/cpu/vm_state_virtualization.h>
#include <translation_table.h>
#include <kernel/configuration.h>

namespace Kernel { class Cpu; }

namespace Board {
	using namespace Hw::Arndale_board;

	using Pic = Hw::Gicv2;

	enum { VCPU_MAX = 1 };

	using Vm_state = Genode::Vm_state;
	using Vm_page_table = Hw::Level_1_stage_2_translation_table;
	using Vm_page_table_array =
		Vm_page_table::Allocator::Array<Kernel::DEFAULT_TRANSLATION_TABLE_MAX>;

	struct Vcpu_context { Vcpu_context(Kernel::Cpu &) {} };
}

#endif /* _CORE__SPEC__ARNDALE__BOARD_H_ */
