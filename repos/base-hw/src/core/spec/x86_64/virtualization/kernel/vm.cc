/*
 * \brief   Kernel backend for x86 virtual machines
 * \author  Benjamin Lamowski
 * \date    2022-10-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <cpu/vm_state_virtualization.h>
#include <util/mmio.h>
#include <cpu/string.h>

#include <hw/assert.h>
#include <map_local.h>
#include <platform_pd.h>
#include <platform.h>
#include <kernel/cpu.h>
#include <kernel/vm.h>
#include <kernel/main.h>

#include <spec/x86_64/virtualization/hypervisor.h>
#include <spec/x86_64/virtualization/svm.h>
#include <hw/spec/x86_64/x86_64.h>

using Genode::addr_t;
using Kernel::Cpu;
using Kernel::Vm;
using Board::Vmcb;


Vm::Vm(Irq::Pool              & user_irq_pool,
       Cpu                    & cpu,
       Genode::Vm_state       & state,
       Kernel::Signal_context & context,
       Identity               & id)
:
	Kernel::Object { *this },
	Cpu_job(Cpu_priority::min(), 0),
	_user_irq_pool(user_irq_pool),
	_state(state),
	_context(context),
	_id(id),
	_vcpu_context(cpu)
{
	affinity(cpu);

}


Vm::~Vm()
{
}


void Vm::proceed(Cpu &)
{
}


void Vm::exception(Cpu &)
{
}


Board::Vcpu_context::Vcpu_context(Cpu &)
:
	vmcb(0),
	regs(1)
{
}
