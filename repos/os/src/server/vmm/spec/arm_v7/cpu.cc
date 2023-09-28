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
#include <state.h>

using Vmm::Cpu_base;
using Vmm::Cpu;
using Vmm::Gic;
using namespace Genode;

addr_t Vmm::Vcpu_state::reg(addr_t idx) const
{
	if (idx > 15) return 0;

	addr_t * r = (addr_t*)this;
	r += idx;
	return *r;
}


void Vmm::Vcpu_state::reg(addr_t idx, addr_t v)
{
	if (idx > 15) return;

	addr_t * r = (addr_t*)this;
	r += idx;
	*r = v;
}


Cpu_base::System_register::Iss::access_t
Cpu_base::System_register::Iss::value(unsigned, unsigned crn, unsigned op1,
                                      unsigned crm, unsigned op2)
{
	access_t v = 0;
	Crn::set(v, crn);
	Crm::set(v, crm);
	Opcode1::set(v, op1);
	Opcode2::set(v, op2);
	return v;
};


Cpu_base::System_register::Iss::access_t
Cpu_base::System_register::Iss::mask_encoding(access_t v)
{
	return Crm::masked(v) |
	       Crn::masked(v) |
	       Opcode1::masked(v) |
	       Opcode2::masked(v);
}


void Cpu_base::_handle_brk(Vcpu_state &)
{
	error(__func__, " not implemented yet");
}


void Cpu_base::handle_exception(Vcpu_state & state)
{
	/* check exception reason */
	switch (state.cpu_exception) {
	case Cpu::NO_EXCEPTION:                 break;
	case Cpu::FIQ:          [[fallthrough]];
	case Cpu::IRQ:          _handle_irq(state);  break;
	case Cpu::TRAP:         _handle_sync(state); break;
	case VCPU_EXCEPTION_STARTUP: _handle_startup(state); break;
	default:
		throw Exception("Curious exception ",
		                state.cpu_exception, " occured");
	}
	state.cpu_exception = Cpu::NO_EXCEPTION;
}


void Cpu_base::dump(Vcpu_state &state)
{
	auto lambda = [] (unsigned i) {
		switch (i) {
		case 0:   return "und";
		case 1:   return "svc";
		case 2:   return "abt";
		case 3:   return "irq";
		case 4:   return "fiq";
		default:  return "unknown";
		};
	};

	log("VM state (", _active ? "active" : "inactive", ") :");
	for (unsigned i = 0; i < 13; i++) {
		log("  r", i, "         = ",
		    Hex(state.reg(i), Hex::PREFIX, Hex::PAD));
	}
	log("  sp         = ", Hex(state.sp,      Hex::PREFIX, Hex::PAD));
	log("  lr         = ", Hex(state.lr,      Hex::PREFIX, Hex::PAD));
	log("  ip         = ", Hex(state.ip,      Hex::PREFIX, Hex::PAD));
	log("  cpsr       = ", Hex(state.cpsr,    Hex::PREFIX, Hex::PAD));
	for (unsigned i = 0; i < Vcpu_state::Mode_state::MAX; i++) {
		log("  sp_", lambda(i), "     = ",
			Hex(state.mode[i].sp, Hex::PREFIX, Hex::PAD));
		log("  lr_", lambda(i), "     = ",
			Hex(state.mode[i].lr, Hex::PREFIX, Hex::PAD));
		log("  spsr_", lambda(i), "   = ",
			Hex(state.mode[i].spsr, Hex::PREFIX, Hex::PAD));
	}
	log("  exception  = ", state.cpu_exception);
	log("  esr_el2    = ", Hex(state.esr_el2,   Hex::PREFIX, Hex::PAD));
	log("  hpfar_el2  = ", Hex(state.hpfar_el2, Hex::PREFIX, Hex::PAD));
	log("  far_el2    = ", Hex(state.far_el2,   Hex::PREFIX, Hex::PAD));
	log("  hifar      = ", Hex(state.hifar,     Hex::PREFIX, Hex::PAD));
	log("  dfsr       = ", Hex(state.dfsr,      Hex::PREFIX, Hex::PAD));
	log("  ifsr       = ", Hex(state.ifsr,      Hex::PREFIX, Hex::PAD));
	log("  sctrl      = ", Hex(state.sctrl,     Hex::PREFIX, Hex::PAD));
	_timer.dump(state);
}


void Cpu_base::initialize_boot(Vcpu_state &state, addr_t ip, addr_t dtb)
{
	state.reg(1, 0xffffffff); /* invalid machine type */
	state.reg(2, dtb);
	state.ip = ip;
}


addr_t Cpu::Ccsidr::read() const
{
	struct Csselr : Register<32>
	{
		struct Level : Bitfield<1, 4> {};
	};

	enum { INVALID = 0xffffffff };

	unsigned level = Csselr::Level::get(csselr.read());

	if (level > 6) {
		warning("Invalid Csselr value!");
		return INVALID;
	}

	return 0;
}

void Cpu::setup_state(Vcpu_state &state)
{
	state.cpsr   = 0x93; /* el1 mode and IRQs disabled */
	state.sctrl  = 0xc50078;
	state.vmpidr = (1UL << 31) | cpu_id();
}


Cpu::Cpu(Vm              & vm,
         Vm_connection   & vm_session,
         Mmio_bus        & bus,
         Gic             & gic,
         Env             & env,
         Heap            & heap,
         Entrypoint      & ep,
         unsigned          id)
: Cpu_base(vm, vm_session, bus, gic, env, heap, ep, id),
  _sr_midr   (0, 0, 0, 0, "MIDR",   false, 0x412fc0f1,     _reg_tree),
  _sr_mpidr  (0, 0, 0, 5, "MPIDR",  false, 1<<31|cpu_id(), _reg_tree),
  _sr_mmfr0  (0, 0, 1, 4, "MMFR0",  false, 0x10201105,     _reg_tree),
  _sr_mmfr1  (0, 0, 1, 5, "MMFR1",  false, 0x20000000,     _reg_tree),
  _sr_mmfr2  (0, 0, 1, 6, "MMFR2",  false, 0x01240000,     _reg_tree),
  _sr_mmfr3  (0, 0, 1, 7, "MMFR3",  false, 0x02102211,     _reg_tree),
  _sr_isar0  (0, 0, 2, 0, "ISAR0",  false, 0x02101110,     _reg_tree),
  _sr_isar1  (0, 0, 2, 1, "ISAR1",  false, 0x13112111,     _reg_tree),
  _sr_isar2  (0, 0, 2, 2, "ISAR2",  false, 0x21232041,     _reg_tree),
  _sr_isar3  (0, 0, 2, 3, "ISAR3",  false, 0x11112131,     _reg_tree),
  _sr_isar4  (0, 0, 2, 4, "ISAR4",  false, 0x10011142,     _reg_tree),
  _sr_isar5  (0, 0, 2, 5, "ISAR5",  false, 0x0,            _reg_tree),
  _sr_pfr0   (0, 0, 1, 0, "PFR0",   false, 0x00001131,     _reg_tree),
  _sr_pfr1   (0, 0, 1, 1, "PFR1",   false, 0x00011011,     _reg_tree),
  _sr_clidr  (0, 1, 0, 1, "CLIDR",  false, 0xa200023,      _reg_tree),
  _sr_csselr (0, 2, 0, 0, "CSSELR", true,  0x0,            _reg_tree),
  _sr_ctr    (0, 0, 0, 1, "CTR",    true,  0x8444c004,     _reg_tree),
  _sr_revidr (0, 0, 0, 6, "REVIDR", true,  0x0,            _reg_tree),
  _sr_ccsidr (_sr_csselr, _reg_tree),
  _sr_actlr  (1, 0, 0, 1, "ACTLR",  true,  0x0,            _reg_tree)
{
}
