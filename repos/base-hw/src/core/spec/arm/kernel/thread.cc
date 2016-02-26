/*
 * \brief   Kernel backend for execution contexts in userland
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2013-11-11
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <assert.h>
#include <kernel/thread.h>
#include <kernel/pd.h>
#include <kernel/kernel.h>

using namespace Kernel;

void Kernel::Thread::_init() { cpu_exception = RESET; }


void Thread::exception(unsigned const cpu)
{
	switch (cpu_exception) {
	case SUPERVISOR_CALL:
		_call();
		return;
	case PREFETCH_ABORT:
	case DATA_ABORT:
		_mmu_exception();
		return;
	case INTERRUPT_REQUEST:
	case FAST_INTERRUPT_REQUEST:
		_interrupt(cpu);
		return;
	case UNDEFINED_INSTRUCTION:
		if (_cpu->retry_undefined_instr(*this)) { return; }
		PWRN("%s -> %s: undefined instruction at ip=%p",
		     pd_label(), label(), (void*)ip);
		_stop();
		return;
	case RESET:
		return;
	default:
		PWRN("%s -> %s: triggered an unknown exception %lu",
		     pd_label(), label(), (unsigned long)cpu_exception);
		_stop();
		return;
	}
}


void Thread::_mmu_exception()
{
	_become_inactive(AWAITS_RESUME);
	if (in_fault(_fault_addr, _fault_writes)) {
		_fault_pd     = (addr_t)_pd->platform_pd();
		_fault_signal = (addr_t)_fault.signal_context();

		/**
		 * core should never raise a page-fault,
		 * if this happens print out an error message with debug information
		 */
		if (_pd == Kernel::core_pd())
			PERR("Pagefault in core thread (%s): ip=%p fault=%p",
			     label(), (void*)ip, (void*)_fault_addr);

		_fault.submit();
		return;
	}
	PERR("%s -> %s: raised unhandled %s DFSR=0x%08x ISFR=0x%08x "
	     "DFAR=0x%08x ip=0x%08lx sp=0x%08lx", pd_label(), label(),
	     cpu_exception == DATA_ABORT ? "data abort" : "prefetch abort",
	     Cpu::Dfsr::read(), Cpu::Ifsr::read(), Cpu::Dfar::read(), ip, sp);
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
	if (!_core()) {
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
	if (!_core()) {
		cpu->clean_invalidate_data_cache();
		cpu->invalidate_instr_cache();
		return;
	}
	auto base = (addr_t)user_arg_1();
	auto const size = (size_t)user_arg_2();
	cpu->clean_invalidate_data_cache_by_virt_region(base, size);
	cpu->invalidate_instr_cache_by_virt_region(base, size);
}


void Thread_event::_signal_acknowledged()
{
	/*
	 * FIXME: this is currently only called as reply to a page-fault resolution.
	 *        On some ARM platforms, we have to do maintainance operations
	 *        after new page table entries where added. If core runs completely
	 *        in privileged mode, we should move this hook to the mappings
	 *        functions.
	 */
	cpu_pool()->cpu(Cpu::executing_id())->translation_table_insertions();
	_thread->_resume();
}

