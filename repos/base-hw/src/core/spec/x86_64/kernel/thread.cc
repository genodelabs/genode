/*
 * \brief   Kernel back-end for execution contexts in userland
 * \author  Martin Stein
 * \author  Reto Buerki
 * \author  Stefan Kalkowski
 * \date    2013-11-11
 */

/*
 * Copyright (C) 2013-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/thread.h>
#include <kernel/pd.h>
#include <kernel/main.h>
#include <platform.h>

#include <hw/spec/x86_64/acpi.h>

using namespace Kernel;

void Thread::Tlb_invalidation::execute(Cpu &)
{
	/* invalidate cpu-local TLB */
	Cpu::invalidate_tlb();

	/* if this is the last cpu, wake up the caller thread */
	if (--cnt == 0) {
		global_work_list.remove(&_le);
		caller._restart();
	}
}


void Thread::Flush_and_stop_cpu::execute(Cpu &cpu)
{
	if (--cpus_left == 0) {
		/* last CPU triggers final ACPI suspend outside kernel lock */
		cpu.suspend.typ_a = suspend.typ_a;
		cpu.suspend.typ_b = suspend.typ_b;
		cpu.next_state_suspend();
		return;
	}

	/* halt CPU outside kernel lock */
	cpu.next_state_halt();

	/* adhere to ACPI specification */
	asm volatile ("wbinvd" : : : "memory");
}


void Cpu::Halt_job::Halt_job::proceed()
{
	switch (_cpu().state()) {
	case HALT:
		while (true) {
			asm volatile ("hlt"); }
		break;
	case SUSPEND:
		using Core::Platform;

		Platform::apply_with_boot_info([&](auto const &boot_info) {
			auto table = boot_info.plat_info.acpi_fadt;
			auto acpi_fadt_table = reinterpret_cast<Hw::Acpi_generic *>(Platform::mmio_to_virt(table));

			/* paranoia */
			if (!acpi_fadt_table)
				return;

			/* all CPUs signaled that they are stopped, trigger ACPI suspend */
			Hw::Acpi_fadt fadt(acpi_fadt_table);

			/* ack all GPEs, otherwise we may wakeup immediately */
			fadt.clear_gpe0_status();
			fadt.clear_gpe1_status();

			/* adhere to ACPI specification */
			asm volatile ("wbinvd" : : : "memory");

			fadt.suspend(_cpu().suspend.typ_a, _cpu().suspend.typ_b);

			Genode::raw("kernel: unexpected resume");
		});
		break;
	default:
		break;
	}

	Genode::raw("unknown cpu state");
	while (true) {
		asm volatile ("hlt");
	}
}


Cpu_suspend_result Thread::_call_cpu_suspend(unsigned sleep_type)
{
	using Genode::uint8_t;
	using Core::Platform;

	Hw::Acpi_generic * acpi_fadt_table { };
	unsigned           cpu_count       { };

	Platform::apply_with_boot_info([&](auto const &boot_info) {
		auto table = boot_info.plat_info.acpi_fadt;
		if (table)
			acpi_fadt_table = reinterpret_cast<Hw::Acpi_generic *>(Platform::mmio_to_virt(table));

		cpu_count = boot_info.cpus;
	});

	if (!acpi_fadt_table || !cpu_count)
		return Cpu_suspend_result::FAILED;

	if (_stop_cpu.constructed()) {
		if (_stop_cpu->cpus_left) {
			Genode::raw("kernel: resume still ongoing");
			return Cpu_suspend_result::FAILED;
		}

		/* remove & destruct Flush_and_stop_cpu object */
		_stop_cpu.destruct();
		return Cpu_suspend_result::OK;
	}

	auto const sleep_typ_a = uint8_t(sleep_type);
	auto const sleep_typ_b = uint8_t(sleep_type >> 8);

	_stop_cpu.construct(_cpu_pool.work_list(), cpu_count - 1,
	                    Hw::Suspend_type { sleep_typ_a, sleep_typ_b });

	/* single core CPU case */
	if (cpu_count == 1) {
		/* current CPU triggers final ACPI suspend outside kernel lock */
		_cpu().next_state_suspend();
		return Cpu_suspend_result::OK;
	}

	/* trigger IPIs to all beside current CPU */
	_cpu_pool.for_each_cpu([&] (Cpu &cpu) {

		if (cpu.id() == Cpu::executing_id()) {
			/* halt CPU outside kernel lock */
			cpu.next_state_halt();
			return;
		}

		cpu.trigger_ip_interrupt();
	});

	return Cpu_suspend_result::OK;
}


void Thread::_call_cache_coherent(addr_t const, size_t const) { }


void Thread::_call_cache_clean_invalidate(addr_t const, size_t const) { }


void Thread::_call_cache_invalidate(addr_t const, size_t const) { }


size_t Thread::_call_cache_line_size()
{
	return 0;
}


void Thread::exception(Genode::Cpu_state &state)
{
	using Genode::Cpu_state;

	_save(state);

	switch (state.trapno) {

	case Cpu_state::PAGE_FAULT:
		_mmu_exception();
		return;

	case Cpu_state::DIVIDE_ERROR:
	case Cpu_state::DEBUG:
	case Cpu_state::BREAKPOINT:
	case Cpu_state::UNDEFINED_INSTRUCTION:
	case Cpu_state::GENERAL_PROTECTION:
		_exception();
		return;

	case Cpu_state::SUPERVISOR_CALL:
		_call();
		return;
	}

	if (state.trapno >= Cpu_state::INTERRUPTS_START &&
	    state.trapno <= Cpu_state::INTERRUPTS_END) {
		_interrupt(_user_irq_pool);
		return;
	}

	_die("unknown exception triggered trapno: ", state.trapno,
	     " with error code=", state.errcode,
	     " at ip=", (void*)state.ip, " sp=", (void*)state.sp);
}


void Thread::proceed()
{
	Cpu::Ia32_tsc_aux::write((Cpu::Ia32_tsc_aux::access_t)_cpu().id().value);

	if (!_cpu().active(_pd.mmu_regs) && type() != CORE)
		_cpu().switch_to(_pd.mmu_regs);

	asm volatile("fxrstor (%1)    \n"
	             "mov  %0, %%rsp  \n"
	             "popq %%r8       \n"
	             "popq %%r9       \n"
	             "popq %%r10      \n"
	             "popq %%r11      \n"
	             "popq %%r12      \n"
	             "popq %%r13      \n"
	             "popq %%r14      \n"
	             "popq %%r15      \n"
	             "popq %%rax      \n"
	             "popq %%rbx      \n"
	             "popq %%rcx      \n"
	             "popq %%rdx      \n"
	             "popq %%rdi      \n"
	             "popq %%rsi      \n"
	             "popq %%rbp      \n"
	             "add  $16, %%rsp \n"
	             "iretq           \n"
	             :: "r" (&regs->r8), "r" (&regs->fpu_context()));
}


void Thread::user_ret_time(Kernel::time_t const t) { regs->rdi = t; }
