/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
 * \author  Sebastian Sumpf
 * \date    2015-06-02
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <assert.h>
#include <kernel/cpu.h>
#include <kernel/pd.h>

using namespace Kernel;

extern Genode::addr_t _mt_client_context_ptr;

struct Mstatus : Genode::Register<64>
{
	enum {
		USER       = 0,
		SUPERVISOR = 1,
		MACHINE    = 3,
		Sv39       = 9,
	};
	struct Ie    : Bitfield<0, 1> { };
	struct Priv  : Bitfield<1, 2> { };
	struct Ie1   : Bitfield<3, 1> { };
	struct Priv1 : Bitfield<4, 2> { };
	struct Fs    : Bitfield<12, 2> { enum { INITIAL = 1 }; };
	struct Vm    : Bitfield<17, 5> { };
	struct Mprv  : Bitfield<16, 1> { };
};


void Kernel::Cpu::init(Kernel::Pic &pic, Kernel::Pd & core_pd,
                       Genode::Board & board)
{
	/* read status register */
	Mstatus::access_t mstatus = 0;

	Mstatus::Vm::set(mstatus, Mstatus::Sv39);         /* enable Sv39 paging  */
	Mstatus::Fs::set(mstatus, Mstatus::Fs::INITIAL);  /* enable FPU          */
	Mstatus::Ie1::set(mstatus, 1);                    /* user mode interrupt */
	Mstatus::Priv1::set(mstatus, Mstatus::USER);      /* set user mode       */
	Mstatus::Ie::set(mstatus, 0);                     /* disable interrupts  */
	Mstatus::Priv::set(mstatus, Mstatus::SUPERVISOR); /* set supervisor mode */

	addr_t client_context_ptr_off = (addr_t)&_mt_client_context_ptr & 0xfff;
	addr_t client_context_ptr     = exception_entry | client_context_ptr_off;
	asm volatile ("csrw sasid,   %0\n" /* address space id  */
	              "csrw sptbr,   %1\n" /* set page table    */
	              "csrw mstatus, %2\n" /* change mode       */
	              "csrw stvec,   %3\n" /* exception vector  */
	              "csrw sscratch,%4\n" /* master conext ptr */
	              :
	              : "r" (core_pd.asid),
	                "r" (core_pd.translation_table()),
	                "r" (mstatus),
	                "r" (exception_entry),
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
