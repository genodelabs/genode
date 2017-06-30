/*
 * \brief   Kernel backend for virtual machines
 * \author  Stefan Kalkowski
 * \author  Reto Buerki
 * \author  Adrian-Ken Rueegsegger
 * \date    2015-06-03
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <assertion.h>
#include <platform_pd.h>
#include <kernel/vm.h>
#include <cpu/cpu_state.h>
#include <pic.h>

Kernel::Vm::Vm(void * const state, Kernel::Signal_context * const context,
               void * const)
: Cpu_job(Cpu_priority::MIN, 0),
  _state((Genode::Vm_state * const) state),
  _context(context),
  _table(nullptr)
{
	affinity(cpu_pool()->primary_cpu());

	/*
	 * Initialize VM context as a core/kernel context to prevent
	 * page-table switching before doing the world switch
	 */
	regs->init((addr_t)core_pd()->translation_table(), true);
}


Kernel::Vm::~Vm() { }


void Kernel::Vm::exception(unsigned const cpu_id)
{
	pause();
	if (regs->trapno == 200) {
		_context->submit(1);
		return;
	}

	if (regs->trapno >= Genode::Cpu_state::INTERRUPTS_START &&
		regs->trapno <= Genode::Cpu_state::INTERRUPTS_END) {
		pic()->irq_occurred(regs->trapno);
		_interrupt(cpu_id);
		_context->submit(1);
		return;
	}
	Genode::warning("VM: triggered unknown exception ", regs->trapno,
	                " with error code ", regs->errcode);

	ASSERT_NEVER_CALLED;
}


extern void * __tss_client_context_ptr;

void Kernel::Vm::proceed(unsigned const cpu_id)
{
	void * * tss_stack_ptr = (&__tss_client_context_ptr);
	*tss_stack_ptr = &regs->cr3;

	asm volatile("sti          \n"
	             "mov $1, %rax \n"
	             "vmcall");
}


void Kernel::Vm::inject_irq(unsigned irq) { }
