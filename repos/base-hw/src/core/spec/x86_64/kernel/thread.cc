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

#include <hw/spec/x86_64/acpi.h>
#include <platform_pd.h>

#include <kernel/main.h>


void Kernel::Thread::Tlb_invalidation::execute(Cpu &)
{
	/* invalidate cpu-local TLB */
	Cpu::invalidate_tlb();

	/* if this is the last cpu, wake up the caller thread */
	if (--cnt == 0) {
		global_work_list.remove(&_le);
		caller._restart();
	}
}


void Kernel::Thread::Flush_and_stop_cpu::execute(Cpu &cpu)
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


void Kernel::Cpu::Halt_job::Halt_job::proceed(Kernel::Cpu &cpu)
{
	switch (cpu.state()) {
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

			fadt.suspend(cpu.suspend.typ_a, cpu.suspend.typ_b);

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


void Kernel::Thread::_call_suspend()
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

	if (!acpi_fadt_table || !cpu_count) {
		user_arg_0(0 /* fail */);
		return;
	}

	if (_stop_cpu.constructed()) {
		if (_stop_cpu->cpus_left) {
			Genode::raw("kernel: resume still ongoing");
			user_arg_0(0 /* fail */);
			return;
		}

		/* remove & destruct Flush_and_stop_cpu object */
		_stop_cpu.destruct();
		user_arg_0(1 /* success */);

		return;
	}

	auto const sleep_typ_a = uint8_t(user_arg_1());
	auto const sleep_typ_b = uint8_t(user_arg_1() >> 8);

	_stop_cpu.construct(_cpu_pool.work_list(), cpu_count - 1,
	                    Hw::Suspend_type { sleep_typ_a, sleep_typ_b });

	/* single core CPU case */
	if (cpu_count == 1) {
		/* current CPU triggers final ACPI suspend outside kernel lock */
		_cpu->next_state_suspend();
		return;
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
}


void Kernel::Thread::_call_cache_coherent_region() { }


void Kernel::Thread::_call_cache_clean_invalidate_data_region() { }


void Kernel::Thread::_call_cache_invalidate_data_region() { }


void Kernel::Thread::_call_cache_line_size()
{
	user_arg_0(0);
}


void Kernel::Thread::proceed(Cpu & cpu)
{
	if (!cpu.active(pd().mmu_regs) && type() != CORE)
		cpu.switch_to(pd().mmu_regs);

	cpu.switch_to(*regs);

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
	             :: "r" (&regs->r8), "r" (regs->fpu_context()));
}


void Kernel::Thread::user_ret_time(Kernel::time_t const t)  { regs->rdi = t;   }
void Kernel::Thread::user_arg_0(Kernel::Call_arg const arg) { regs->rdi = arg; }
void Kernel::Thread::user_arg_1(Kernel::Call_arg const arg) { regs->rsi = arg; }
void Kernel::Thread::user_arg_2(Kernel::Call_arg const arg) { regs->rdx = arg; }
void Kernel::Thread::user_arg_3(Kernel::Call_arg const arg) { regs->rcx = arg; }
void Kernel::Thread::user_arg_4(Kernel::Call_arg const arg) { regs->r8 = arg; }
void Kernel::Thread::user_arg_5(Kernel::Call_arg const arg) { regs->r9 = arg; }

Kernel::Call_arg Kernel::Thread::user_arg_0() const { return regs->rdi; }
Kernel::Call_arg Kernel::Thread::user_arg_1() const { return regs->rsi; }
Kernel::Call_arg Kernel::Thread::user_arg_2() const { return regs->rdx; }
Kernel::Call_arg Kernel::Thread::user_arg_3() const { return regs->rcx; }
Kernel::Call_arg Kernel::Thread::user_arg_4() const { return regs->r8; }
Kernel::Call_arg Kernel::Thread::user_arg_5() const { return regs->r9; }
