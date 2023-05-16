/*
 * \brief   Kernel backend for x86 virtual machines
 * \author  Benjamin Lamowski
 * \date    2022-10-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <cpu/vm_state_virtualization.h>
#include <util/construct_at.h>
#include <util/mmio.h>
#include <cpu/string.h>

#include <hw/assert.h>
#include <map_local.h>
#include <platform_pd.h>
#include <platform.h>
#include <kernel/cpu.h>
#include <kernel/vm.h>
#include <kernel/main.h>

#include <virtualization/hypervisor.h>
#include <virtualization/svm.h>
#include <hw/spec/x86_64/x86_64.h>

using Genode::addr_t;
using Kernel::Cpu;
using Kernel::Vm;
using Board::Vmcb;
using Vcpu_run_state = Genode::Vcpu_run_state;


Vm::Vm(Irq::Pool              & user_irq_pool,
       Cpu                    & cpu,
       Genode::Vm_data        & data,
       Kernel::Signal_context & context,
       Identity               & id)
:
	Kernel::Object { *this },
	Cpu_job(Cpu_priority::min(), 0),
	_user_irq_pool(user_irq_pool),
	_state(*data.vm_state),
	_context(context),
	_id(id),
	_vcpu_context(id.id, &data.vmcb, data.vmcb_phys_addr)
{
	affinity(cpu);
	_state.run_state.set(Vcpu_run_state::STARTUP);
}


Vm::~Vm()
{
}


void Vm::proceed(Cpu & cpu)
{
	using namespace Board;
	cpu.switch_to(*_vcpu_context.regs);

	bool do_world_switch = false;

	switch (_state.run_state.value()) {
		case Vcpu_run_state::STARTUP: break;
		case Vcpu_run_state::SYNC_FROM_VCPU: break;
		case Vcpu_run_state::PAUSING: break;
		case Vcpu_run_state::INTERRUPTIBLE:
			if (_state.run_state.cas(Vcpu_run_state::INTERRUPTIBLE,
			                         Vcpu_run_state::RUNNING))
				do_world_switch = true;
			break;
		case Vcpu_run_state::RUNNABLE:
			_state.run_state.cas(Vcpu_run_state::RUNNABLE,
			                        Vcpu_run_state::RUNNING);
			[[fallthrough]];
		case Vcpu_run_state::RUN_ONCE:
			_vcpu_context.read_vcpu_state(_state);
			do_world_switch = true;
			break;
		default:
			Genode::error("proceed: illegal state ",
			              Genode::Hex(_state.run_state.value()));
	}

	if (do_world_switch) {
		Cpu::Ia32_tsc_aux::write((Cpu::Ia32_tsc_aux::access_t) _vcpu_context.tsc_aux_guest);

		/*
		 * We push the host context's physical address to trapno so that
		 * we can pop it later
		 * */
		_vcpu_context.regs->trapno = _vcpu_context.vmcb.root_vmcb_phys;
		Hypervisor::switch_world(_vcpu_context.vmcb.phys_addr,
		                          (addr_t)&_vcpu_context.regs->r8,
		                          _vcpu_context.regs->fpu_context());
		/*
		 * This will fall into an interrupt or otherwise jump into
		 * _kernel_entry
		 * */
	} else {
		_vcpu_context.regs->trapno = TRAP_VMSKIP;
		Hypervisor::restore_state_for_entry((addr_t)&_vcpu_context.regs->r8,
		                                    _vcpu_context.regs->fpu_context());
		/* jumps to _kernel_entry */
	}
}

void Vm::exception(Cpu & cpu)
{
	using namespace Board;
	using Genode::Cpu_state;

	switch (_vcpu_context.regs->trapno) {
		case Cpu_state::INTERRUPTS_START ... Cpu_state::INTERRUPTS_END:
			_interrupt(_user_irq_pool, cpu.id());
			break;
		case TRAP_VMEXIT:
			/* exception method was entered because of a VMEXIT */
			break;
		case TRAP_VMSKIP:
			/* exception method was entered without exception */
			break;
		default:
			Genode::error("VM: triggered unknown exception ",
			              _vcpu_context.regs->trapno,
			              " with error code ", _vcpu_context.regs->errcode,
			              " at ip=",
			              (void *)_vcpu_context.regs->ip, " sp=",
			              (void *)_vcpu_context.regs->sp);
			pause();
			return;
	};

	enum Svm_exitcodes : Genode::uint64_t {
		VMEXIT_INVALID = -1ULL,
		VMEXIT_INTR = 0x60,
		VMEXIT_NPF = 0x400,
	};

	switch (_state.run_state.value()) {
		case Vcpu_run_state::STARTUP:
			_vcpu_context.initialize_svm(cpu, _id.table);
			_vcpu_context.tsc_aux_host = cpu.id();
			_vcpu_context.write_vcpu_state(_state, EXIT_STARTUP);
			_state.run_state.set(Vcpu_run_state::DISPATCHING);
			pause();
			_context.submit(1);
			return;
		case Vcpu_run_state::SYNC_FROM_VCPU:
			_vcpu_context.write_vcpu_state(_state, EXIT_PAUSED);
			_state.run_state.set(Vcpu_run_state::PAUSED);
			pause();
			_context.submit(1);
			return;
		case Vcpu_run_state::EXITING: break;
		case Vcpu_run_state::RUNNING: break;
		case Vcpu_run_state::RUN_ONCE: break;
		case Vcpu_run_state::PAUSING: return;
		default:
			Genode::error("exception: illegal state ",
			              Genode::Hex(_state.run_state.value()));
	}

	Genode::uint64_t exitcode = _vcpu_context.vmcb.read<Vmcb::Exitcode>();

	switch (exitcode) {
		case VMEXIT_INVALID:
			Genode::error("Vm::exception: invalid SVM state!");
			return;
		case 0x40 ... 0x5f:
			Genode::error("Vm::exception: unhandled SVM exception ",
			              Genode::Hex(exitcode));
			return;
		case VMEXIT_INTR:
			if (!_state.run_state.cas(Vcpu_run_state::RUNNING,
			                          Vcpu_run_state::INTERRUPTIBLE))
			{
				_vcpu_context.write_vcpu_state(_state, EXIT_PAUSED);

				/*
				 * If the interruptible state couldn't be set, the state might
				 * be EXITING and a pause() signal might have already been send
				 * (to cause the vCPU exit in the first place).
				 */
				bool submit = false;
				/* In the RUN_ONCE case, first we will need to send a signal. */
				if (_state.run_state.value() == Vcpu_run_state::RUN_ONCE)
					submit = true;

				_state.run_state.set(Vcpu_run_state::PAUSED);
				pause();

				if (submit)
					_context.submit(1);
			}
			return;
		case VMEXIT_NPF:
			exitcode = EXIT_NPF;
			[[fallthrough]];
		default:
			_vcpu_context.write_vcpu_state(_state, (unsigned) exitcode);
			_state.run_state.set(Vcpu_run_state::DISPATCHING);
			pause();
			_context.submit(1);
			return;
	};
}


Board::Vcpu_context::Vcpu_context(unsigned id, void *vcpu_data_ptr,
                                  Genode::addr_t context_phys_addr)
:
	vmcb(*Genode::construct_at<Vmcb>(vcpu_data_ptr, id, context_phys_addr)),
	regs(1)
{
	regs->trapno = TRAP_VMEXIT;
}
