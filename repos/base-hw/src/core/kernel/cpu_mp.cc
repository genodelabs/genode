/*
 * \brief  Kernel cpu object implementations for multiprocessor systems
 * \author Stefan Kalkowski
 * \date   2018-11-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <kernel/cpu.h>

using namespace Kernel;

void Cpu::Ipi::occurred()
{
	/* lambda to iterate over a work-list and execute all work items */
	auto iterate = [] (Genode::List<Genode::List_element<Inter_processor_work>> & li) {
		Genode::List_element<Inter_processor_work> const *e = li.first();
		Genode::List_element<Inter_processor_work> const *next = nullptr;
		for ( ; e; e = next) {
			next = e->next();
			e->object()->execute();
		}
	};

	/* iterate through the local and global work-list */
	iterate(cpu._local_work_list);
	iterate(cpu._global_work_list);

	/* mark the IPI as being received */
	pending = false;
}


void Cpu::trigger_ip_interrupt()
{
	/* check whether there is still an IPI send */
	if (_ipi_irq.pending) return;

	_pic.send_ipi(_id);
	_ipi_irq.pending = true;
}


Cpu::Ipi::Ipi(Cpu & cpu)
: Irq(Board::Pic::IPI, cpu), cpu(cpu) {
	cpu.pic().unmask(Board::Pic::IPI, cpu.id()); }
