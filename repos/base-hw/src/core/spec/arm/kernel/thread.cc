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

/* base includes */
#include <cpu/memory_barrier.h>

/* base-hw core includes */
#include <platform_pd.h>
#include <kernel/cpu.h>
#include <kernel/pd.h>
#include <kernel/thread.h>

using namespace Kernel;

extern "C" void kernel_to_user_context_switch(Core::Cpu::Context*,
                                              Core::Cpu::Fpu_context*);


void Thread::_call_suspend() { }


void Thread::exception()
{
	using Ctx = Core::Cpu::Context;

	switch (regs->cpu_exception) {
	case Ctx::SUPERVISOR_CALL:
		_call();
		return;
	case Ctx::PREFETCH_ABORT:
	case Ctx::DATA_ABORT:
		_mmu_exception();
		return;
	case Ctx::INTERRUPT_REQUEST:
	case Ctx::FAST_INTERRUPT_REQUEST:
		_interrupt(_user_irq_pool);
		return;
	case Ctx::UNDEFINED_INSTRUCTION:
		Genode::raw(*this, ": undefined instruction at ip=",
		            Genode::Hex(regs->ip));
		_die();
		return;
	case Ctx::RESET:
		return;
	default:
		Genode::raw(*this, ": triggered an unknown exception ",
		            regs->cpu_exception);
		_die();
		return;
	}
}


/**
 * on ARM with multiprocessing extensions, maintainance operations on TLB,
 * and caches typically work coherently across CPUs when using the correct
 * coprocessor registers (there might be ARM SoCs where this is not valid,
 * with several shareability domains, but until now we do not support them)
 */
void Kernel::Thread::Tlb_invalidation::execute(Cpu &) { }


void Thread::Flush_and_stop_cpu::execute(Cpu &) { }


void Cpu::Halt_job::proceed() { }


void Thread::proceed()
{
	if (!_cpu().active(pd().mmu_regs) && type() != CORE)
		_cpu().switch_to(pd().mmu_regs);

	regs->cpu_exception = _cpu().stack_start();
	kernel_to_user_context_switch((static_cast<Core::Cpu::Context*>(&*regs)),
	                              (static_cast<Core::Cpu::Fpu_context*>(&*regs)));
}


void Thread::user_ret_time(Kernel::time_t const t)
{
	/* split 64-bit time_t value into 2 register */
	regs->r0 = (addr_t) (t >> 32UL);
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
