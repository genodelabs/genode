/*
 * \brief  Kernel backend for x86 virtual machines
 * \author Benjamin Lamowski
 * \date   2022-10-14
 */

/*
 * Copyright (C) 2022-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <cpu/vcpu_state_virtualization.h>
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


Vm::Vm(Irq::Pool              & user_irq_pool,
       Cpu                    & cpu,
       Genode::Vcpu_data      & data,
       Kernel::Signal_context & context,
       Identity               & id)
:
	Kernel::Object { *this },
	Cpu_job(Cpu_priority::min(), 0),
	_user_irq_pool(user_irq_pool),
	_state(*data.vcpu_state),
	_context(context),
	_id(id),
	_vcpu_context(id.id, data.vmcb)
{
	affinity(cpu);
}


Vm::~Vm()
{
}


void Vm::proceed(Cpu & cpu)
{
	using namespace Board;
	cpu.switch_to(*_vcpu_context.regs);

	if (_vcpu_context.exitcode == EXIT_INIT) {
		_vcpu_context.regs->trapno = TRAP_VMSKIP;
		Hypervisor::restore_state_for_entry((addr_t)&_vcpu_context.regs->r8,
		                                    _vcpu_context.regs->fpu_context());
		/* jumps to _kernel_entry */
	}

	Cpu::Ia32_tsc_aux::write(
	    (Cpu::Ia32_tsc_aux::access_t)_vcpu_context.tsc_aux_guest);

	/*
	 * We push the host context's physical address to trapno so that
	 * we can pop it later
	 */
	_vcpu_context.regs->trapno = _vcpu_context.vmcb.root_vmcb_phys;
	Hypervisor::switch_world(_vcpu_context.vmcb.vcpu_data()->vmcb_phys_addr,
	                         (addr_t) &_vcpu_context.regs->r8,
	                         _vcpu_context.regs->fpu_context());
	/*
	 * This will fall into an interrupt or otherwise jump into
	 * _kernel_entry
	 */
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
			_pause_vcpu();
			return;
	};

	enum Svm_exitcodes : Genode::uint64_t {
		VMEXIT_INVALID = -1ULL,
		VMEXIT_INTR = 0x60,
		VMEXIT_NPF = 0x400,
	};

	if (_vcpu_context.exitcode == EXIT_INIT) {
			_vcpu_context.initialize_svm(cpu, _id.table);
			_vcpu_context.tsc_aux_host = cpu.id();
			_vcpu_context.exitcode     = EXIT_STARTUP;
			_pause_vcpu();
			_context.submit(1);
			return;
	}

	_vcpu_context.exitcode = _vcpu_context.vmcb.read<Vmcb::Exitcode>();

	switch (_vcpu_context.exitcode) {
		case VMEXIT_INVALID:
			Genode::error("Vm::exception: invalid SVM state!");
			return;
		case 0x40 ... 0x5f:
			Genode::error("Vm::exception: unhandled SVM exception ",
			              Genode::Hex(_vcpu_context.exitcode));
			return;
		case VMEXIT_INTR:
			_vcpu_context.exitcode = EXIT_PAUSED;
			return;
		case VMEXIT_NPF:
			_vcpu_context.exitcode = EXIT_NPF;
			[[fallthrough]];
		default:
			_pause_vcpu();
			_context.submit(1);
			return;
	}
}


void Vm::_sync_to_vmm()
{
	_vcpu_context.write_vcpu_state(_state);

	/*
	 * Set exit code so that if _run() was not called after an exit, the
	 * next exit due to a signal will be interpreted as PAUSE request.
	 */
	_vcpu_context.exitcode = Board::EXIT_PAUSED;
}


void Vm::_sync_from_vmm()
{
	/* first run() will skip through to issue startup exit */
	if (_vcpu_context.exitcode == Board::EXIT_INIT)
		return;

	_vcpu_context.read_vcpu_state(_state);
}


Board::Vcpu_context::Vcpu_context(unsigned id, void *vcpu_data_ptr)
:
	vmcb(*Genode::construct_at<Vmcb>(vcpu_data_ptr, id)),
	regs(1)
{
	regs->trapno = TRAP_VMEXIT;
}
