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

#include <kernel/cpu.h>
#include <kernel/kernel.h>
#include <kernel/pd.h>
#include <kernel/thread.h>

using namespace Kernel;

void Kernel::Thread::_init()
{
	init(_core);
}


void Thread::exception(unsigned const cpu)
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
		_interrupt(cpu);
		return;
	case Cpu::Context::UNDEFINED_INSTRUCTION:
		if (_cpu->retry_undefined_instr(*this)) { return; }
		Genode::warning(*this, ": undefined instruction at ip=",
		                Genode::Hex(regs->ip));
		_die();
		return;
	case Cpu::Context::RESET:
		return;
	default:
		Genode::warning(*this, ": triggered an unknown exception ",
		                regs->cpu_exception);
		_die();
		return;
	}
}


void Thread::_mmu_exception()
{
	_become_inactive(AWAITS_RESTART);
	if (in_fault(_fault_addr, _fault_writes, _fault_exec)) {
		_fault_pd = (addr_t)_pd->platform_pd();

		/*
		 * Core should never raise a page-fault. If this happens, print out an
		 * error message with debug information.
		 */
		if (_pd == Kernel::core_pd())
			Genode::error("page fault in core thread (", label(), "): "
			              "ip=", Genode::Hex(regs->ip), " fault=", Genode::Hex(_fault_addr));

		if (_pager) _pager->submit(1);
		return;
	}

	char const *abort_type = "unknown";
	if (regs->cpu_exception == Cpu::Context::DATA_ABORT)
		abort_type = "data";
	if (regs->cpu_exception == Cpu::Context::PREFETCH_ABORT)
		abort_type = "prefetch";

	Genode::error(*this, ": raised unhandled ",
	              abort_type, " abort ",
	              "DFSR=", Genode::Hex(Cpu::Dfsr::read()), " "
	              "ISFR=", Genode::Hex(Cpu::Ifsr::read()), " "
	              "DFAR=", Genode::Hex(Cpu::Dfar::read()), " "
	              "ip=",   Genode::Hex(regs->ip),          " "
	              "sp=",   Genode::Hex(regs->sp),          " "
	              "exception=", Genode::Hex(regs->cpu_exception));
}


void Kernel::Thread::_call_update_data_region()
{
	Cpu * const cpu  = cpu_pool()->cpu(Cpu::executing_id());

	/*
	 * FIXME: If the caller is not a core thread, the kernel operates in a
	 *        different address space than the caller. Combined with the fact
	 *        that at least ARMv7 doesn't provide cache operations by physical
	 *        address, this prevents us from selectively maintaining caches.
	 *        The future solution will be a kernel that is mapped to every
	 *        address space so we can use virtual addresses of the caller. Up
	 *        until then we apply operations to caches as a whole instead.
	 */
	if (!_core) {
		cpu->clean_invalidate_data_cache();
		return;
	}
	auto base = (addr_t)user_arg_1();
	auto const size = (size_t)user_arg_2();
	cpu->clean_invalidate_data_cache_by_virt_region(base, size);
	cpu->invalidate_instr_cache();
}


void Kernel::Thread::_call_update_instr_region()
{
	Cpu * const cpu  = cpu_pool()->cpu(Cpu::executing_id());

	/*
	 * FIXME: If the caller is not a core thread, the kernel operates in a
	 *        different address space than the caller. Combined with the fact
	 *        that at least ARMv7 doesn't provide cache operations by physical
	 *        address, this prevents us from selectively maintaining caches.
	 *        The future solution will be a kernel that is mapped to every
	 *        address space so we can use virtual addresses of the caller. Up
	 *        until then we apply operations to caches as a whole instead.
	 */
	if (!_core) {
		cpu->clean_invalidate_data_cache();
		cpu->invalidate_instr_cache();
		return;
	}
	auto base = (addr_t)user_arg_1();
	auto const size = (size_t)user_arg_2();
	cpu->clean_invalidate_data_cache_by_virt_region(base, size);
	cpu->invalidate_instr_cache_by_virt_region(base, size);
}


extern void * kernel_stack;

void Thread::proceed(unsigned const cpu)
{
	regs->cpu_exception = (addr_t)&kernel_stack + Cpu::KERNEL_STACK_SIZE * (cpu+1);

	asm volatile("mov  sp, %0        \n"
	             "msr  spsr_cxsf, %1 \n"
	             "mov  lr, %2        \n"
	             "ldm  sp, {r0-r14}^ \n"
	             "subs pc, lr, #0    \n"
	             :: "r" (static_cast<Cpu::Context*>(&*regs)),
	                "r" (regs->cpsr), "r" (regs->ip));
}
