/*
 * \brief   Kernel backend for virtual machines
 * \author  Stefan Kalkowski
 * \date    2015-06-03
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <platform_pd.h>
#include <kernel/vm.h>

Kernel::Vm::Vm(void * const, Kernel::Signal_context * const context,
               void * const)
: Cpu_job(Cpu_priority::MIN, 0),
  _state(nullptr),
  _context(context),
  _table(nullptr)
{
	affinity(cpu_pool()->primary_cpu());
}


Kernel::Vm::~Vm() { }


void Kernel::Vm::exception(unsigned const cpu_id)
{
	PDBG("Implement me please");
}


void Kernel::Vm::proceed(unsigned const cpu_id)
{
	PDBG("Implement me please");
}


void Kernel::Vm::inject_irq(unsigned irq) { }
