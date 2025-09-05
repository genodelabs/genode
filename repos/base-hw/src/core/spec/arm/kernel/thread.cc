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
#include <kernel/cpu.h>
#include <kernel/pd.h>
#include <kernel/thread.h>

using namespace Kernel;

extern "C" void kernel_to_user_context_switch(Board::Cpu::Fpu_context*,
                                              Board::Cpu::Context*, void*);


Cpu_suspend_result Thread::_call_cpu_suspend(unsigned const) {
	return Cpu_suspend_result::FAILED; }


void Thread::exception(Genode::Cpu_state &state)
{
	using Ctx = Board::Cpu::Context;

	Genode::memcpy(&*regs, &state, sizeof(Ctx));

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
		_die("Undefined instruction at ip=", Genode::Hex(regs->ip));
		return;
	case Ctx::RESET:
		return;
	default:
		_die("Unknown exception triggered: ", regs->cpu_exception);
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
	if (!_cpu().active(_pd.mmu_regs) && type() != CORE)
		_cpu().switch_to(_pd.mmu_regs);

	kernel_to_user_context_switch((static_cast<Board::Cpu::Fpu_context*>(&*regs)),
	                              (static_cast<Board::Cpu::Context*>(&*regs)),
	                              (void*)_cpu().stack_start());
}


void Thread::user_ret_time(Kernel::time_t const t)
{
	/* split 64-bit time_t value into 2 register */
	regs->r0 = (addr_t) (t >> 32UL);
	regs->r1 = t & ~0UL;
}
