/*
 * \brief  Arm virt board driver for core
 * \author Piotr Tworek
 * \date   2019-09-15
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__CORE__SPEC__VIRT__QEMU_H_
#define _SRC__CORE__SPEC__VIRT__QEMU_H_

#include <hw/spec/arm/virt_qemu_board.h>
#include <spec/arm/virtualization/gicv2.h>
#include <spec/arm/generic_timer.h>
#include <spec/arm/cpu/vm_state_virtualization.h>
#include <translation_table.h>
#include <kernel/configuration.h>

namespace Kernel { class Cpu; }

namespace Board {
	using namespace Hw::Virt_qemu_board;

	struct Virtual_local_pic {};

	enum {
		TIMER_IRQ           = 30 /* PPI IRQ 14 */,
		VT_TIMER_IRQ        = 27,
		VT_MAINTAINANCE_IRQ = 25,
		VCPU_MAX            = 1
	};

	using Vm_state = Genode::Vm_state;
	using Vm_page_table = Hw::Level_1_stage_2_translation_table;
	using Vm_page_table_array =
		Vm_page_table::Allocator::Array<Kernel::DEFAULT_TRANSLATION_TABLE_MAX>;

	struct Vcpu_context { Vcpu_context(Kernel::Cpu &) {} };
};

#endif /* _SRC__CORE__SPEC__VIRT__QEMU_H_ */
