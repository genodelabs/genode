/*
 * \brief  VMM cpu object
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2019-07-18
 */

/*
 * Copyright (C) 2019-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#include <cpu.h>
#include <vm.h>
#include <psci.h>

using Vmm::Cpu_base;
using Vmm::Cpu;
using Vmm::Gic;


Cpu_base::System_register::System_register(unsigned         op0,
                                           unsigned         crn,
                                           unsigned         op1,
                                           unsigned         crm,
                                           unsigned         op2,
                                           const char     * name,
                                           bool             writeable,
                                           Genode::addr_t   v,
                                           Genode::Avl_tree<System_register> & tree)
: _encoding(Iss::value(op0, crn, op1, crm, op2)),
  _name(name),
  _writeable(writeable),
  _value(v)
{
	tree.insert(this);
}


bool Cpu_base::_handle_sys_reg(Vcpu_state & state)
{
	using Iss = System_register::Iss;

	Iss::access_t v = state.esr_el2;
	System_register * reg = _reg_tree.first();
	if (reg) reg = reg->find_by_encoding(Iss::mask_encoding(v));

	if (!reg) {
		Genode::error("ignore unknown system register access @ ip=", (void*)state.ip, ":");
		Genode::error(Iss::Direction::get(v) ? "read" : "write",
		              ": "
		              "op0=",  Iss::Opcode0::get(v), " "
		              "op1=",  Iss::Opcode1::get(v), " "
		              "r",    Iss::Register::get(v),   " "
		              "crn=",  Iss::Crn::get(v),        " "
		              "crm=",  Iss::Crm::get(v), " ",
		              "op2=",  Iss::Opcode2::get(v));
		if (Iss::Direction::get(v)) state.reg(Iss::Register::get(v), 0);
		state.ip += sizeof(Genode::uint32_t);
		return false;
	}

	if (Iss::Direction::get(v)) { /* read access  */
		state.reg(Iss::Register::get(v), reg->read());
	} else {                      /* write access */
		if (!reg->writeable()) {
			Genode::error("writing to system register ",
			              reg->name(), " not allowed!");
			return false;
		}
		reg->write(state.reg(Iss::Register::get(v)));
	}
	state.ip += sizeof(Genode::uint32_t);
	return true;
}


void Cpu_base::_handle_wfi(Vcpu_state &state)
{
	state.ip += sizeof(Genode::uint32_t);

	if (state.esr_el2 & 1) return; /* WFE */

	_active = false;
	_timer.schedule_timeout(state);
}


void Cpu_base::_handle_startup(Vcpu_state &state)
{
	Generic_timer::setup_state(state);
	Gic::Gicd_banked::setup_state(state);

	setup_state(state);

	if (cpu_id() == 0) {
		initialize_boot(state, _vm.kernel_addr(), _vm.dtb_addr());
	} else {
		_cpu_ready.down();
	}
	_active = true;
}


void Cpu_base::_handle_sync(Vcpu_state &state)
{
	/* check device number*/
	switch (Esr::Ec::get(state.esr_el2)) {
	case Esr::Ec::HVC_32: [[fallthrough]];
	case Esr::Ec::HVC:
		_handle_hyper_call(state);
		break;
	case Esr::Ec::MRC_MCR: [[fallthrough]];
	case Esr::Ec::MRS_MSR:
		_handle_sys_reg(state);
		break;
	case Esr::Ec::DA:
		_handle_data_abort(state);
		break;
	case Esr::Ec::WFI:
		_handle_wfi(state);
		return;
	case Esr::Ec::BRK:
		_handle_brk(state);
		return;
	default:
		throw Exception("Unknown trap: ",
		                Esr::Ec::get(state.esr_el2));
	};
}


void Cpu_base::_handle_irq(Vcpu_state &state)
{
	switch (state.irqs.last_irq) {
	case VTIMER_IRQ:
		_timer.handle_irq(state);
		break;
	default:
		_gic.handle_irq(state);
	};
}


void Cpu_base::_handle_hyper_call(Vcpu_state &state)
{
	switch(state.reg(0)) {
		case Psci::PSCI_VERSION:
			state.reg(0, Psci::VERSION);
			return;
		case Psci::MIGRATE_INFO_TYPE:
			state.reg(0, Psci::NOT_SUPPORTED);
			return;
		case Psci::PSCI_FEATURES:
			state.reg(0, Psci::NOT_SUPPORTED);
			return;
		case Psci::CPU_ON_32: [[fallthrough]];
		case Psci::CPU_ON:
			_vm.cpu((unsigned)state.reg(1), [&] (Cpu & cpu) {
				Vcpu_state & local_state = cpu.state();
				cpu.initialize_boot(local_state, state.reg(2), state.reg(3));
				cpu.set_ready();
			});
			state.reg(0, Psci::SUCCESS);
			return;
		default:
			Genode::warning("unknown hypercall! ", cpu_id());
			dump(state);
	};
}


void Cpu_base::_handle_data_abort(Vcpu_state &state)
{
	_vm.bus().handle_memory_access(state, *static_cast<Cpu *>(this));
	state.ip += sizeof(Genode::uint32_t);
}


void Cpu_base::_update_state(Vcpu_state &state)
{
	if (!_gic.pending_irq(state)) return;

	_active = true;
	_timer.cancel_timeout();
}

unsigned Cpu_base::cpu_id() const         { return _vcpu_id;  }
bool Cpu_base::active() const             { return _active;   }
Gic::Gicd_banked & Cpu_base::gic()        { return _gic;      }


void Cpu_base::recall()
{
	Genode::Signal_transmitter(_vm_handler).submit();
};


Cpu_base::Cpu_base(Vm                      & vm,
                   Genode::Vm_connection   & vm_session,
                   Mmio_bus                & bus,
                   Gic                     & gic,
                   Genode::Env             & env,
                   Genode::Heap            & heap,
                   Genode::Entrypoint      & ep,
                   unsigned                  id)
: _vcpu_id(id),
  _vm(vm),
  _vm_session(vm_session),
  _heap(heap),
  _vm_handler(*this, ep, *this, &Cpu_base::_handle_nothing),
  _vm_vcpu(_vm_session, heap, _vm_handler, _exit_config),
  _gic(*this, gic, bus),
  _timer(env, ep, _gic.irq(VTIMER_IRQ), *this) { }
