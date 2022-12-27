/*
 * \brief   Kernel backend for execution contexts in userland
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
#include <kernel/cpu.h>
#include <kernel/pd.h>
#include <kernel/thread.h>

using namespace Kernel;


void Thread::Tlb_invalidation::execute() {}


void Thread::exception(Cpu & cpu)
{
	using Context = Genode::Cpu::Context;
	using Stval = Genode::Cpu::Stval;

	if (regs->is_irq()) {
		/* cpu-local timer interrupt */
		if (regs->irq() == cpu.timer().interrupt_id()) {
			cpu.handle_if_cpu_local_interrupt(cpu.timer().interrupt_id());
		} else {
			/* interrupt controller */
			_interrupt(_user_irq_pool, 0);
		}
		return;
	}

	switch(regs->cpu_exception) {
	case Context::ECALL_FROM_SUPERVISOR:
		_call();
		break;
	case Context::ECALL_FROM_USER:
		_call();
		regs->ip += 4; /* set to next instruction */
		break;
	case Context::INSTRUCTION_PAGE_FAULT:

		/*
		 * Quirk for MIG-V:
		 *
		 * On MIG-V 'stval' does not report the correct address for instructions
		 * that cross a page boundary.
		 *
		 * Spec 1.10: "For instruction-fetch access faults and page faults on RISC-V
		 * systems with variable-length instructions, stval will point to the
		 * portion of the instruction that caused the fault while sepc will point to
		 * the beginning of the instruction."
		 *
		 * On MIG-V stval always points to the beginning of the instruction.
		 *
		 * Save the last instruction-fetch fault in 'last_fetch_fault', in case the
		 * next fetch fault occurs at the same IP and is at a page border, set
		 * page-fault address ('stval') to next page.
		 */
		if (regs->last_fetch_fault == regs->ip && (regs->ip & 0xfff) == 0xffe)
			Stval::write(Stval::read() + 4);

		_mmu_exception();
		regs->last_fetch_fault = regs->ip;

		break;

	case Context::STORE_PAGE_FAULT:
	case Context::LOAD_PAGE_FAULT:
	case Context::INSTRUCTION_ACCESS_FAULT:
	case Context::LOAD_ACCESS_FAULT:
	case Context::STORE_ACCESS_FAULT:
		_mmu_exception();
		break;
	default:
		Genode::raw(*this, ": unhandled exception ", regs->cpu_exception,
		            " at ip=", (void*)regs->ip,
		            " addr=", Genode::Hex(Genode::Cpu::Stval::read()));
		_die();
	}
}


void Thread::_call_cache_coherent_region() { }


void Kernel::Thread::_call_cache_clean_invalidate_data_region() { }


void Kernel::Thread::_call_cache_invalidate_data_region() { }


void Kernel::Thread::proceed(Cpu & cpu)
{
	/*
	 * The sstatus register defines to which privilege level
	 * the machine returns when doing an exception return
	 */
	Cpu::Sstatus::access_t v = Cpu::Sstatus::read();
	Cpu::Sstatus::Spp::set(v, (type() == USER) ? 0 : 1);
	Cpu::Sstatus::write(v);

	if (!cpu.active(pd().mmu_regs) && type() != CORE)
		cpu.switch_to(_pd->mmu_regs);

	asm volatile("csrw sscratch, %1                                \n"
	             "mv   x31, %0                                     \n"
	             "ld   x30, (x31)                                  \n"
	             "csrw sepc, x30                                   \n"
	             ".irp reg,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,"
	                      "18,19,20,21,22,23,24,25,26,27,28,29,30  \n"
	             "  ld x\\reg, 8 * (\\reg + 1)(x31)                \n"
	             ".endr                                            \n"
	             "csrrw x31, sscratch, x31                         \n"
	             "sret                                             \n"
	             :: "r" (&*regs), "r" (regs->t6) : "x30", "x31");
}


void Thread::user_ret_time(Kernel::time_t const t)  { regs->a0  = t;   }
void Thread::user_arg_0(Kernel::Call_arg const arg) { regs->a0  = arg; }
void Thread::user_arg_1(Kernel::Call_arg const arg) { regs->a1  = arg; }
void Thread::user_arg_2(Kernel::Call_arg const arg) { regs->a2  = arg; }
void Thread::user_arg_3(Kernel::Call_arg const arg) { regs->a3  = arg; }
void Thread::user_arg_4(Kernel::Call_arg const arg) { regs->a4  = arg; }
void Thread::user_arg_5(Kernel::Call_arg const arg) { regs->a5  = arg; }
Kernel::Call_arg Thread::user_arg_0() const { return regs->a0; }
Kernel::Call_arg Thread::user_arg_1() const { return regs->a1; }
Kernel::Call_arg Thread::user_arg_2() const { return regs->a2; }
Kernel::Call_arg Thread::user_arg_3() const { return regs->a3; }
Kernel::Call_arg Thread::user_arg_4() const { return regs->a4; }
Kernel::Call_arg Thread::user_arg_5() const { return regs->a5; }
