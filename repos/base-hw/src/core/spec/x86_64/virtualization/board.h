/*
 * \brief  Board with PC virtualization support
 * \author Benjamin Lamowski
 * \date   2022-10-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__PC__VIRTUALIZATION__BOARD_H_
#define _CORE__SPEC__PC__VIRTUALIZATION__BOARD_H_

/* base-hw core includes */
#include <kernel/configuration.h>
#include <kernel/irq.h>

#include <hw/spec/x86_64/page_table.h>
#include <cpu/vm_state_virtualization.h>

namespace Board {

	using Vm_page_table = Hw::Page_table;
	using Vm_page_table_array =
		Vm_page_table::Allocator::Array<Kernel::DEFAULT_TRANSLATION_TABLE_MAX>;

	struct Vcpu_context;

	using Vm_state = Genode::Vm_state;

	enum {
		VCPU_MAX            = 16
	};
};


namespace Kernel {
	class Cpu;
	class Vm;
};


struct Board::Vcpu_context
{
	Vcpu_context(Kernel::Cpu & cpu);
};

#endif /* _CORE__SPEC__PC__VIRTUALIZATION__BOARD_H_ */
