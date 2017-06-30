/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
 * \author  Sebastian Sumpf
 * \date    2015-06-02
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <assertion.h>
#include <kernel/cpu.h>
#include <kernel/pd.h>

#include <hw/memory_map.h>

using namespace Kernel;

void Kernel::Cpu::init(Kernel::Pic &pic) {
	Stvec::write(Hw::Mm::supervisor_exception_vector().base); }


Cpu_idle::Cpu_idle(Cpu * const cpu) : Cpu_job(Cpu_priority::MIN, 0)
{
	Cpu_job::cpu(cpu);
	regs->cpu_exception = Cpu::Context::RESET;
	regs->ip = (addr_t)&_main;
	regs->sp = (addr_t)&_stack[stack_size];
	init_thread((addr_t)core_pd()->translation_table(), core_pd()->asid);
}


void Cpu_idle::exception(unsigned const cpu)
{
	if (regs->is_irq()) {
		_interrupt(cpu);
		return;
	} else if (regs->cpu_exception == Cpu::Context::RESET) return;

	ASSERT_NEVER_CALLED;
}


void Cpu_idle::proceed(unsigned const)
{
	asm volatile("csrw sscratch, %1                                \n"
	             "mv   x31, %0                                     \n"
	             "ld   x30, (x31)                                  \n"
	             "csrw sepc, x30                                   \n"
	             ".irp reg,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,"
	                      "18,19,20,21,22,23,24,25,26,27,28,29,30  \n"
	             "  ld x\\reg, 8 * (\\reg + 1)(x31)                \n"
	             ".endr                                            \n"
	             "csrrw x31, sscratch, x31                         \n"
	             "sret                                             \n"
	             :: "r" (&*regs), "r" (regs->t6) : "x30", "x31");
}
