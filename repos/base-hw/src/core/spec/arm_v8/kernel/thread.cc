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

#include <platform_pd.h>

#include <kernel/cpu.h>
#include <kernel/pd.h>
#include <kernel/thread.h>

#include <hw/memory_map.h>

extern "C" void kernel_to_user_context_switch(void *, void *);

using namespace Kernel;


void Thread::_call_suspend() { }


void Thread::exception(Cpu & cpu)
{
	switch (regs->exception_type) {
	case Cpu::RESET:         return;
	case Cpu::IRQ_LEVEL_EL0: [[fallthrough]];
	case Cpu::IRQ_LEVEL_EL1: [[fallthrough]];
	case Cpu::FIQ_LEVEL_EL0: [[fallthrough]];
	case Cpu::FIQ_LEVEL_EL1:
		_interrupt(_user_irq_pool, cpu.id());
		return;
	case Cpu::SYNC_LEVEL_EL0: [[fallthrough]];
	case Cpu::SYNC_LEVEL_EL1:
		{
			Cpu::Esr::access_t esr = Cpu::Esr_el1::read();
			switch (Cpu::Esr::Ec::get(esr)) {
			case Cpu::Esr::Ec::SVC:
				_call();
				return;
			case Cpu::Esr::Ec::INST_ABORT_SAME_LEVEL: [[fallthrough]];
			case Cpu::Esr::Ec::DATA_ABORT_SAME_LEVEL:
				Genode::raw("Fault in kernel/core ESR=", Genode::Hex(esr));
				[[fallthrough]];
			case Cpu::Esr::Ec::INST_ABORT_LOW_LEVEL:  [[fallthrough]];
			case Cpu::Esr::Ec::DATA_ABORT_LOW_LEVEL:
				_mmu_exception();
				return;
			case Cpu::Esr::Ec::SOFTWARE_STEP_LOW_LEVEL: [[fallthrough]];
			case Cpu::Esr::Ec::BRK:
				regs->ec = Cpu::Esr::Ec::get(esr);
				_exception();
				return;
			default:
				Genode::raw("Unknown cpu exception EC=", Cpu::Esr::Ec::get(esr),
				            " ISS=", Cpu::Esr::Iss::get(esr),
				            " ip=", (void*)regs->ip);
			};
			
			/*
			 * If the machine exception is caused by a non-privileged
			 * component, mark it dead, and continue execution.
			 */
			if (regs->exception_type == Cpu::SYNC_LEVEL_EL0) {
				Genode::raw("Will freeze thread ", *this);
				_become_inactive(DEAD);
				return;
			}
			break;
		}
	default:
		Genode::raw("Exception vector: ", (void*)regs->exception_type,
					" not implemented!");
	};

	while (1) { ; }
}


/**
 * on ARM with multiprocessing extensions, maintainance operations on TLB,
 * and caches typically work coherently across CPUs when using the correct
 * coprocessor registers (there might be ARM SoCs where this is not valid,
 * with several shareability domains, but until now we do not support them)
 */
void Kernel::Thread::Tlb_invalidation::execute(Cpu &) { }


void Thread::Flush_and_stop_cpu::execute(Cpu &) { }


void Cpu::Halt_job::proceed(Kernel::Cpu &) { }


bool Kernel::Pd::invalidate_tlb(Cpu & cpu, addr_t addr, size_t size)
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
		for (addr_t end = addr+size; addr < end; addr += get_page_size())
			asm volatile ("tlbi vaae1is, %0" :: "r" (addr >> 12));
		return false;
	}

	/**
	 * Too big mappings will result in long running invalidation loops,
	 * just invalidate the whole tlb for the ASID then.
	 */
	if (size > 8 * get_page_size()) {
		asm volatile ("tlbi aside1is, %0"
		              :: "r" ((uint64_t)mmu_regs.id() << 48));
		return false;
	}

	for (addr_t end = addr+size; addr < end; addr += get_page_size())
		asm volatile ("tlbi vae1is, %0"
		              :: "r" (addr >> 12 | (uint64_t)mmu_regs.id() << 48));
	return false;
}


void Thread::proceed(Cpu & cpu)
{
	if (!cpu.active(pd().mmu_regs) && type() != CORE)
		cpu.switch_to(pd().mmu_regs);

	kernel_to_user_context_switch((static_cast<Cpu::Context*>(&*regs)),
	                              (void*)cpu.stack_start());
}


void Thread::user_ret_time(Kernel::time_t const t)  { regs->r[0] = t;   }
void Thread::user_arg_0(Kernel::Call_arg const arg) { regs->r[0] = arg; }
void Thread::user_arg_1(Kernel::Call_arg const arg) { regs->r[1] = arg; }
void Thread::user_arg_2(Kernel::Call_arg const arg) { regs->r[2] = arg; }
void Thread::user_arg_3(Kernel::Call_arg const arg) { regs->r[3] = arg; }
void Thread::user_arg_4(Kernel::Call_arg const arg) { regs->r[4] = arg; }
void Thread::user_arg_5(Kernel::Call_arg const arg) { regs->r[5] = arg; }

Kernel::Call_arg Thread::user_arg_0() const { return regs->r[0]; }
Kernel::Call_arg Thread::user_arg_1() const { return regs->r[1]; }
Kernel::Call_arg Thread::user_arg_2() const { return regs->r[2]; }
Kernel::Call_arg Thread::user_arg_3() const { return regs->r[3]; }
Kernel::Call_arg Thread::user_arg_4() const { return regs->r[4]; }
Kernel::Call_arg Thread::user_arg_5() const { return regs->r[5]; }
