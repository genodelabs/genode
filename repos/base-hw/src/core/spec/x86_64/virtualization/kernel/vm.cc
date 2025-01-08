/*
 * \brief  Kernel backend for x86 virtual machines
 * \author Benjamin Lamowski
 * \date   2022-10-14
 */

/*
 * Copyright (C) 2022-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#include <base/log.h>
#include <cpu/vcpu_state_virtualization.h>
#include <util/construct_at.h>
#include <util/mmio.h>
#include <cpu/string.h>

#include <map_local.h>
#include <platform_pd.h>
#include <platform.h>
#include <kernel/cpu.h>
#include <kernel/vm.h>
#include <kernel/main.h>

#include <hw/spec/x86_64/x86_64.h>
#include <virtualization/hypervisor.h>
#include <virtualization/svm.h>
#include <virtualization/vmx.h>

using namespace Genode;

using Kernel::Cpu;
using Kernel::Vm;


Vm::Vm(Irq::Pool              & user_irq_pool,
       Cpu                    & cpu,
       Vcpu_data              & data,
       Kernel::Signal_context & context,
       Identity               & id)
:
	Kernel::Object { *this },
	Cpu_context(cpu, Scheduler::Priority::min(), 0),
	_user_irq_pool(user_irq_pool),
	_state(*data.vcpu_state),
	_context(context),
	_id(id),
	_vcpu_context(id.id, data) { }


Vm::~Vm()
{
}


void Vm::proceed()
{
	using namespace Board;
	_cpu().switch_to(*_vcpu_context.regs);

	if (_vcpu_context.exit_reason == EXIT_INIT) {
		_vcpu_context.regs->trapno = TRAP_VMSKIP;
		Hypervisor::restore_state_for_entry((addr_t)&_vcpu_context.regs->r8,
		                                    _vcpu_context.regs->fpu_context());
		/* jumps to _kernel_entry */
	}

	Cpu::Ia32_tsc_aux::write(
	    (Cpu::Ia32_tsc_aux::access_t)_vcpu_context.tsc_aux_guest);

	_vcpu_context.virt.switch_world(*_vcpu_context.regs);
	/*
	 * This will fall into an interrupt or otherwise jump into
	 * _kernel_entry. If VMX encountered a severe error condition,
	 * it will print an error message and regularly return from the world
	 * switch. In this case, just remove the vCPU thread from the scheduler.
	 */
	_pause_vcpu();
}


void Vm::exception()
{
	using namespace Board;

	bool pause = false;

	switch (_vcpu_context.regs->trapno) {
		case TRAP_VMEXIT:
			_vcpu_context.exit_reason =
				_vcpu_context.virt.handle_vm_exit();
			/*
			 * If handle_vm_exit() returns EXIT_PAUSED, the vCPU has
			 * exited due to a host interrupt. The exit reason is
			 * set to EXIT_PAUSED so that if the VMM queries the
			 * vCPU state while the vCPU is stopped, it is clear
			 * that it does not need to handle a synchronous vCPU exit.
			 *
			 * VMX jumps directly to __kernel_entry when exiting
			 * guest mode and skips the interrupt vectors, therefore
			 * trapno will not be set to the host interrupt and we
			 * have to explicitly handle interrupts here.
			 *
			 * SVM on the other hand will service the host interrupt
			 * after the stgi instruction (see
			 * AMD64 Architecture Programmer's Manual Vol 2
			 * 15.17 Global Interrupt Flag, STGI and CLGI Instructions)
			 * and will jump to the interrupt vector, setting trapno
			 * to the host interrupt. This means the exception
			 * handler should actually skip this case branch, which
			 * is fine because _vcpu_context.exit_reason is set to
			 * EXIT_PAUSED by default, so a VMM querying the vCPU
			 * state will still get the right value.
			 *
			 * For any other exit reason, we exclude this vCPU
			 * thread from being scheduled and signal the VMM that
			 * it needs to handle an exit.
			 */
			if (_vcpu_context.exit_reason == EXIT_PAUSED)
				_interrupt(_user_irq_pool);
			else
				pause = true;
			break;
		case Cpu_state::INTERRUPTS_START ... Cpu_state::INTERRUPTS_END:
			_interrupt(_user_irq_pool);
			break;
		case TRAP_VMSKIP:
			/* vCPU is running for the first time */
			_vcpu_context.initialize(_cpu(),
			    reinterpret_cast<addr_t>(_id.table));
			_vcpu_context.tsc_aux_host = _cpu().id();
			/*
			 * We set the artificial startup exit code, stop the
			 * vCPU thread and ask the VMM to handle it.
			 */
			_vcpu_context.exit_reason = EXIT_STARTUP;
			pause = true;
			break;
		default:
			error("VM: triggered unknown exception ",
			              _vcpu_context.regs->trapno,
			              " with error code ", _vcpu_context.regs->errcode,
			              " at ip=",
			              (void *)_vcpu_context.regs->ip, " sp=",
			              (void *)_vcpu_context.regs->sp);
			_pause_vcpu();
			break;
	};

	if (pause == true) {
		_pause_vcpu();
		_context.submit(1);
	}
}


void Vm::_sync_to_vmm()
{
	_vcpu_context.write_vcpu_state(_state);

	/*
	 * Set exit code so that if _run() was not called after an exit, the
	 * next exit due to a signal will be interpreted as PAUSE request.
	 */
	_vcpu_context.exit_reason = Board::EXIT_PAUSED;
}


void Vm::_sync_from_vmm()
{
	/* first run() will skip through to issue startup exit */
	if (_vcpu_context.exit_reason == Board::EXIT_INIT)
		return;

	_vcpu_context.read_vcpu_state(_state);
}

Board::Vcpu_context::Vcpu_context(unsigned id, Vcpu_data &vcpu_data)
:
	regs(1),
	virt(detect_virtualization(vcpu_data, id))
{
	regs->trapno = TRAP_VMEXIT;
}

void Board::Vcpu_context::read_vcpu_state(Vcpu_state &state)
{
	virt.read_vcpu_state(state);

	if (state.cx.charged() || state.dx.charged() || state.bx.charged()) {
		regs->rax   = state.ax.value();
		regs->rcx   = state.cx.value();
		regs->rdx   = state.dx.value();
		regs->rbx   = state.bx.value();
	}

	if (state.bp.charged() || state.di.charged() || state.si.charged()) {
		regs->rdi   = state.di.value();
		regs->rsi   = state.si.value();
		regs->rbp   = state.bp.value();
	}

	if (state.r8 .charged() || state.r9 .charged() ||
	    state.r10.charged() || state.r11.charged() ||
	    state.r12.charged() || state.r13.charged() ||
	    state.r14.charged() || state.r15.charged()) {

		regs->r8  = state.r8.value();
		regs->r9  = state.r9.value();
		regs->r10 = state.r10.value();
		regs->r11 = state.r11.value();
		regs->r12 = state.r12.value();
		regs->r13 = state.r13.value();
		regs->r14 = state.r14.value();
		regs->r15 = state.r15.value();
	}

	if (state.fpu.charged()) {
		state.fpu.with_state(
		    [&](Vcpu_state::Fpu::State const &fpu) {
			    memcpy((void *) regs->fpu_context(), &fpu, regs->fpu_size());
		    });
	}
}

void Board::Vcpu_context::write_vcpu_state(Vcpu_state &state)
{
	state.discharge();
	state.exit_reason = (unsigned) exit_reason;

	state.fpu.charge([&](Vcpu_state::Fpu::State &fpu) {
		memcpy(&fpu, (void *) regs->fpu_context(), regs->fpu_size());
		return regs->fpu_size();
	});

	/* SVM will overwrite rax but VMX doesn't. */
	state.ax.charge(regs->rax);
	state.cx.charge(regs->rcx);
	state.dx.charge(regs->rdx);
	state.bx.charge(regs->rbx);

	state.di.charge(regs->rdi);
	state.si.charge(regs->rsi);
	state.bp.charge(regs->rbp);

	state.r8.charge(regs->r8);
	state.r9.charge(regs->r9);
	state.r10.charge(regs->r10);
	state.r11.charge(regs->r11);
	state.r12.charge(regs->r12);
	state.r13.charge(regs->r13);
	state.r14.charge(regs->r14);
	state.r15.charge(regs->r15);

	state.tsc.charge(Hw::Tsc::rdtsc());

	tsc_aux_guest = Cpu::Ia32_tsc_aux::read();
	state.tsc_aux.charge(tsc_aux_guest);
	Cpu::Ia32_tsc_aux::write((Cpu::Ia32_tsc_aux::access_t) tsc_aux_host);

	virt.write_vcpu_state(state);
}


void Board::Vcpu_context::initialize(Kernel::Cpu &cpu, addr_t table_phys_addr)
{
	virt.initialize(cpu, table_phys_addr, *regs);
}
