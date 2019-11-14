/*
 * \brief  Board wirh ARM virtualization support
 * \author Stefan Kalkowski
 * \date   2019-11-12
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM__VIRTUALIZATION__BOARD_H_
#define _CORE__SPEC__ARM__VIRTUALIZATION__BOARD_H_

#include <translation_table.h>
#include <kernel/configuration.h>
#include <kernel/irq.h>

namespace Board {
	using Vm_page_table = Hw::Level_1_stage_2_translation_table;
	using Vm_page_table_array =
		Vm_page_table::Allocator::Array<Kernel::DEFAULT_TRANSLATION_TABLE_MAX>;

	struct Vcpu_context;

	using Vm_state = Genode::Vm_state;
};

namespace Kernel {
	class Cpu;
	class Vm;
};

struct Board::Vcpu_context
{
	struct Vm_irq : Kernel::Irq
	{
		Vm_irq(unsigned const irq, Kernel::Cpu &);
		virtual ~Vm_irq() {};

		virtual void handle(Kernel::Cpu &, Kernel::Vm & vm, unsigned irq);
		void occurred() override;
	};


	struct Pic_maintainance_irq : Vm_irq
	{
		Pic_maintainance_irq(Kernel::Cpu &);

		void handle(Kernel::Cpu &, Kernel::Vm &, unsigned) override { }
	};


	struct Virtual_timer_irq
	{
		Vm_irq irq;

		Virtual_timer_irq(Kernel::Cpu &);

		void enable();
		void disable();
	};

	Vcpu_context(Kernel::Cpu & cpu)
	: pic_irq(cpu), vtimer_irq(cpu) {}

	Pic::Virtual_context pic {};
	Pic_maintainance_irq pic_irq;
	Virtual_timer_irq    vtimer_irq;
};

#endif /* _CORE__SPEC__ARM__VIRTUALIZATION__BOARD_H_ */
