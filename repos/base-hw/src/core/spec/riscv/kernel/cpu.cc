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
#include <assert.h>
#include <kernel/cpu.h>
#include <kernel/pd.h>

using namespace Kernel;

extern Genode::addr_t _mt_client_context_ptr;

void Kernel::Cpu::init(Kernel::Pic &pic/*, Kernel::Pd & core_pd,
                       Genode::Board & board*/)
{
	addr_t client_context_ptr_off = (addr_t)&_mt_client_context_ptr & 0xfff;
	addr_t client_context_ptr     = exception_entry | client_context_ptr_off;
	asm volatile ("csrw stvec,   %0\n" /* exception vector  */
	              "csrw sscratch,%1\n" /* master conext ptr */
	              :
	              : "r" (exception_entry),
	                "r" (client_context_ptr)
	              : "memory");
}


Cpu_idle::Cpu_idle(Cpu * const cpu) : Cpu_job(Cpu_priority::MIN, 0)
{
	Cpu_job::cpu(cpu);
	cpu_exception = RESET;
	ip = (addr_t)&_main;
	sp = (addr_t)&_stack[stack_size];
	init_thread((addr_t)core_pd()->translation_table(), core_pd()->asid);
}


void Cpu_idle::exception(unsigned const cpu)
{
	if (is_irq()) {
		_interrupt(cpu);
		return;
	} else if (cpu_exception == RESET) return;

	assert(0);
}
