/*
 * \brief   Kernel backend for execution contexts in userland
 * \author  Stefan Kalkowski
 * \date    2019-05-11
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <kernel/cpu.h>
#include <kernel/pd.h>
#include <kernel/thread.h>

#include <hw/memory_map.h>

extern "C" void kernel_to_user_context_switch(void *, void *);

using namespace Kernel;


Cpu_suspend_result Core_thread::_call_cpu_suspend(unsigned const) {
	return Cpu_suspend_result::FAILED; }


void Thread::exception(Genode::Cpu_state &state)
{
	using namespace Genode;

	_save(state);

	uint64_t type = static_cast<Board::Cpu::Context&>(state).exception_type;

	switch (type) {
	case Cpu::RESET:         return;
	case Cpu::IRQ_LEVEL_EL0: [[fallthrough]];
	case Cpu::IRQ_LEVEL_EL1: [[fallthrough]];
	case Cpu::FIQ_LEVEL_EL0: [[fallthrough]];
	case Cpu::FIQ_LEVEL_EL1:
		_interrupt();
		return;
	case Cpu::SYNC_LEVEL_EL0: [[fallthrough]];
	case Cpu::SYNC_LEVEL_EL1:
		{
			switch (Cpu::Esr::Ec::get(state.esr_el1)) {
			case Cpu::Esr::Ec::SVC:
				_call();
				return;
			case Cpu::Esr::Ec::INST_ABORT_SAME_LEVEL: [[fallthrough]];
			case Cpu::Esr::Ec::DATA_ABORT_SAME_LEVEL:
				raw("Fault in kernel/core ESR=", Hex(state.esr_el1));
				[[fallthrough]];
			case Cpu::Esr::Ec::INST_ABORT_LOW_LEVEL:  [[fallthrough]];
			case Cpu::Esr::Ec::DATA_ABORT_LOW_LEVEL:
				_mmu_exception();
				return;
			case Cpu::Esr::Ec::SOFTWARE_STEP_LOW_LEVEL: [[fallthrough]];
			case Cpu::Esr::Ec::BRK:
				_exception();
				return;
			default:
				raw("Unknown cpu exception EC=", Cpu::Esr::Ec::get(state.esr_el1),
				    " ISS=", Cpu::Esr::Iss::get(state.esr_el1),
				    " ip=", (void*)state.ip);
			};
			
			/*
			 * If the machine exception is caused by a non-privileged
			 * component, mark it dead, and continue execution.
			 */
			if (type == Cpu::SYNC_LEVEL_EL0) {
				_die("Unhandled machine exception caused.");
				return;
			}
			break;
		}
	default:
		raw("Exception vector: ", (void*)type, " not implemented!");
	};

	_cpu().panic(state);
}


/**
 * on ARM with multiprocessing extensions, maintainance operations on TLB,
 * and caches typically work coherently across CPUs when using the correct
 * coprocessor registers (there might be ARM SoCs where this is not valid,
 * with several shareability domains, but until now we do not support them)
 */
void Kernel::Core_thread::Tlb_invalidation::execute(Cpu &) { }


void Core_thread::Flush_and_stop_cpu::execute(Cpu &) { }


void Cpu::Halt_job::proceed() { }


bool Kernel::Pd::invalidate_tlb(Cpu &cpu, addr_t addr, size_t size)
{
	using namespace Genode;

	/* only apply to the active cpu */
	if (cpu.id() != Cpu::executing_id())
		return false;

	/**
	 * The kernel part of the address space is mapped as global
	 * therefore we have to invalidate it differently
	 */
	if (addr >= Hw::Mm::supervisor_exception_vector().base) {
		for (addr_t end = addr+size; addr < end; addr += PAGE_SIZE)
			asm volatile ("tlbi vaae1is, %0" :: "r" (addr >> 12));
		return false;
	}

	/**
	 * Too big mappings will result in long running invalidation loops,
	 * just invalidate the whole tlb for the ASID then.
	 */
	if (size > 8*PAGE_SIZE) {
		asm volatile ("tlbi aside1is, %0"
					  :: "r" ((uint64_t)mmu_regs.id() << 48));
		return false;
	}

	for (addr_t end = addr+size; addr < end; addr += PAGE_SIZE)
		asm volatile ("tlbi vae1is, %0"
					  :: "r" (addr >> 12 | (uint64_t)mmu_regs.id() << 48));
	return false;
}


void Thread::proceed()
{
	if (!_cpu().active(_pd.mmu_regs) && !_privileged())
		_cpu().switch_to(_pd.mmu_regs);

	kernel_to_user_context_switch((static_cast<Board::Cpu::Context*>(&*regs)),
	                              (void*)_cpu().stack_start());
}


void Thread::user_ret_time(Kernel::time_t const t) { regs->r[0] = t; }
