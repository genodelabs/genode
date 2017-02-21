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
		Genode::warning(*this, ": undefined instruction at ip=",
		                Genode::Hex(ip));
		_die();
		return;
	case RESET:
		return;
	default:
		Genode::warning(*this, ": triggered an unknown exception ",
		                cpu_exception);
		_die();
		return;
	}
}


void Thread::_mmu_exception()
{
	_become_inactive(AWAITS_RESTART);
	if (in_fault(_fault_addr, _fault_writes)) {
		_fault_pd = (addr_t)_pd->platform_pd();

		/*
		 * Core should never raise a page-fault. If this happens, print out an
		 * error message with debug information.
		 */
		if (_pd == Kernel::core_pd())
			Genode::error("page fault in core thread (", label(), "): "
			              "ip=", Genode::Hex(ip), " fault=", Genode::Hex(_fault_addr));

		if (_pager) _pager->submit(1);
		return;
	}
	Genode::error(*this, ": raised unhandled ",
	              cpu_exception == DATA_ABORT ? "data abort" : "prefetch abort", " "
	              "DFSR=", Genode::Hex(Cpu::Dfsr::read()), " "
	              "ISFR=", Genode::Hex(Cpu::Ifsr::read()), " "
	              "DFAR=", Genode::Hex(Cpu::Dfar::read()), " "
	              "ip=",   Genode::Hex(ip),                " "
	              "sp=",   Genode::Hex(sp));
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
