/*
 * \brief   Kernel backend for execution contexts in userland
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2013-11-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu/memory_barrier.h>

#include <platform_pd.h>
#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/pd.h>
#include <kernel/thread.h>

using namespace Kernel;

extern "C" void kernel_to_user_context_switch(Cpu::Context*, Cpu::Fpu_context*);


void Thread::exception(Cpu & cpu)
{
	switch (regs->cpu_exception) {
	case Cpu::Context::SUPERVISOR_CALL:
		_call();
		return;
	case Cpu::Context::PREFETCH_ABORT:
	case Cpu::Context::DATA_ABORT:
		_mmu_exception();
		return;
	case Cpu::Context::INTERRUPT_REQUEST:
	case Cpu::Context::FAST_INTERRUPT_REQUEST:
		_interrupt(cpu.id());
		return;
	case Cpu::Context::UNDEFINED_INSTRUCTION:
		Genode::raw(*this, ": undefined instruction at ip=",
		            Genode::Hex(regs->ip));
		_die();
		return;
	case Cpu::Context::RESET:
		return;
	default:
		Genode::raw(*this, ": triggered an unknown exception ",
		            regs->cpu_exception);
		_die();
		return;
	}
}


void Kernel::Thread::_call_cache_coherent_region()
{
	addr_t       base = (addr_t) user_arg_1();
	size_t const size = (size_t) user_arg_2();

	/**
	 * sanity check that only one small page is affected,
	 * because we only want to lookup one page in the page tables
	 * to limit execution time within the kernel
	 */
	if (Hw::trunc_page(base) != Hw::trunc_page(base+size-1)) {
		Genode::raw(*this, " tried to make cross-page region cache coherent ",
		            (void*)base, " ", size);
		return;
	}

	/**
	 * Lookup whether the page is backed, and if so make the memory coherent
	 * in between I-, and D-cache
	 */
	addr_t phys = 0;
	if (pd().platform_pd().lookup_translation(base, phys)) {
		Cpu::cache_coherent_region(base, size);
	} else {
		Genode::raw(*this, " tried to make invalid address ",
		            base, " cache coherent");
	}
}


/**
 * on ARM with multiprocessing extensions, maintainance operations on TLB,
 * and caches typically work coherently across CPUs when using the correct
 * coprocessor registers (there might be ARM SoCs where this is not valid,
 * with several shareability domains, but until now we do not support them)
 */
void Kernel::Thread::Tlb_invalidation::execute() { };


void Thread::proceed(Cpu & cpu)
{
	cpu.switch_to(*regs, pd().mmu_regs);

	regs->cpu_exception = cpu.stack_start();
	kernel_to_user_context_switch((static_cast<Cpu::Context*>(&*regs)),
	                              (static_cast<Cpu::Fpu_context*>(&*regs)));
}


void Thread::user_ret_time(Kernel::time_t const t)
{
	regs->r0 = t >> 32UL;
	regs->r1 = t & ~0UL;
}

void Thread::user_arg_0(Kernel::Call_arg const arg) { regs->r0 = arg; }
void Thread::user_arg_1(Kernel::Call_arg const arg) { regs->r1 = arg; }
void Thread::user_arg_2(Kernel::Call_arg const arg) { regs->r2 = arg; }
void Thread::user_arg_3(Kernel::Call_arg const arg) { regs->r3 = arg; }
void Thread::user_arg_4(Kernel::Call_arg const arg) { regs->r4 = arg; }
void Thread::user_arg_5(Kernel::Call_arg const arg) { regs->r5 = arg; }

Kernel::Call_arg Thread::user_arg_0() const { return regs->r0; }
Kernel::Call_arg Thread::user_arg_1() const { return regs->r1; }
Kernel::Call_arg Thread::user_arg_2() const { return regs->r2; }
Kernel::Call_arg Thread::user_arg_3() const { return regs->r3; }
Kernel::Call_arg Thread::user_arg_4() const { return regs->r4; }
Kernel::Call_arg Thread::user_arg_5() const { return regs->r5; }
