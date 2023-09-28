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
#include <state.h>
#include <vm.h>
#include <psci.h>

using Vmm::Cpu_base;
using Vmm::Cpu;
using Vmm::Gic;
using namespace Genode;

addr_t Vmm::Vcpu_state::reg(addr_t idx) const
{
	if (idx > 30) return 0;
	return r[idx];
}


void Vmm::Vcpu_state::reg(addr_t idx, addr_t v)
{
	if (idx > 30) return;
	r[idx] = v;
}


Cpu_base::System_register::Iss::access_t
Cpu_base::System_register::Iss::value(unsigned op0, unsigned crn, unsigned op1,
                                      unsigned crm, unsigned op2)
{
	access_t v = 0;
	Crn::set(v, crn);
	Crm::set(v, crm);
	Opcode0::set(v, op0);
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
	       Opcode2::masked(v) |
	       Opcode0::masked(v);
}


void Cpu_base::_handle_brk(Vcpu_state & state)
{
	addr_t offset = 0x0;
	if (!(state.pstate & 0b100)) {
		offset = 0x400;
	} else if (state.pstate & 0b1) {
		offset = 0x200;
	}

	/* only the below 32-bit of system register ESR_EL2 and PSTATE are used */
	state.esr_el1  = (uint32_t)state.esr_el2;
	state.spsr_el1 = (uint32_t)state.pstate;
	state.elr_el1  = state.ip;
	state.ip       = state.vbar_el1 + offset;
	state.pstate   = 0b1111000101;
}


void Cpu_base::handle_exception(Vcpu_state &state)
{
	/* check exception reason */
	switch (state.exception_type) {
	case Cpu::NO_EXCEPTION:                 break;
	case Cpu::AARCH64_IRQ:  _handle_irq(state);  break;
	case Cpu::AARCH64_SYNC: _handle_sync(state); break;
	case VCPU_EXCEPTION_STARTUP: _handle_startup(state); break;
	default:
		throw Exception("Curious exception ",
		                state.exception_type, " occured");
	}
	state.exception_type = Cpu::NO_EXCEPTION;
}


void Cpu_base::dump(Vcpu_state &state)
{
	auto lambda = [] (addr_t exc) {
		switch (exc) {
		case Cpu::AARCH64_SYNC:   return "aarch64 sync";
		case Cpu::AARCH64_IRQ:    return "aarch64 irq";
		case Cpu::AARCH64_FIQ:    return "aarch64 fiq";
		case Cpu::AARCH64_SERROR: return "aarch64 serr";
		case Cpu::AARCH32_SYNC:   return "aarch32 sync";
		case Cpu::AARCH32_IRQ:    return "aarch32 irq";
		case Cpu::AARCH32_FIQ:    return "aarch32 fiq";
		case Cpu::AARCH32_SERROR: return "aarch32 serr";
		default:             return "unknown";
		};
	};

	log("VM state (", _active ? "active" : "inactive", ") :");
	for (unsigned i = 0; i < 31; i++) {
		log("  r", i, "         = ",
		    Hex(state.r[i], Hex::PREFIX, Hex::PAD));
	}
	log("  sp         = ", Hex(state.sp,      Hex::PREFIX, Hex::PAD));
	log("  ip         = ", Hex(state.ip,      Hex::PREFIX, Hex::PAD));
	log("  sp_el1     = ", Hex(state.sp_el1,  Hex::PREFIX, Hex::PAD));
	log("  elr_el1    = ", Hex(state.elr_el1, Hex::PREFIX, Hex::PAD));
	log("  pstate     = ", Hex(state.pstate,  Hex::PREFIX, Hex::PAD));
	log("  exception  = ", state.exception_type, " (",
	                       lambda(state.exception_type), ")");
	log("  esr_el2    = ", Hex(state.esr_el2, Hex::PREFIX, Hex::PAD));
	_timer.dump(state);
}


addr_t Cpu::Ccsidr::read() const
{
	Vcpu_state & state = cpu.state();

	struct Clidr : Genode::Register<32>
	{
		enum Cache_entry {
			NO_CACHE,
			INSTRUCTION_CACHE_ONLY,
			DATA_CACHE_ONLY,
			SEPARATE_CACHE,
			UNIFIED_CACHE
		};

		static unsigned level(unsigned l, access_t reg) {
			return (reg >> l*3) & 0b111; }
	};

	struct Csselr : Genode::Register<32>
	{
		struct Instr : Bitfield<0, 1> {};
		struct Level : Bitfield<1, 4> {};
	};

	enum { INVALID = 0xffffffff };

	unsigned level = Csselr::Level::get((Csselr::access_t)csselr.read());
	bool     instr = Csselr::Instr::get((Csselr::access_t)csselr.read());

	if (level > 6) {
		warning("Invalid Csselr value!");
		return INVALID;
	}

	unsigned ce = Clidr::level(level, (Clidr::access_t)state.clidr_el1);

	if (ce == Clidr::NO_CACHE ||
	    (ce == Clidr::DATA_CACHE_ONLY && instr)) {
		warning("Invalid Csselr value!");
		return INVALID;
	}

	if (ce == Clidr::INSTRUCTION_CACHE_ONLY ||
	    (ce == Clidr::SEPARATE_CACHE && instr)) {
		log("Return Ccsidr instr value ", state.ccsidr_inst_el1[level]);
		return state.ccsidr_inst_el1[level];
	}

	log("Return Ccsidr value ", state.ccsidr_data_el1[level]);
	return state.ccsidr_data_el1[level];
}


addr_t Cpu::Ctr_el0::read() const
{
	addr_t ret;
	asm volatile("mrs %0, ctr_el0" : "=r" (ret));
	return ret;
}


void Cpu::Icc_sgi1r_el1::write(addr_t v)
{

	unsigned target_list = v & 0xffff;
	unsigned irq         = (v >> 24) & 0xf;

	vm.for_each_cpu([&] (Cpu & cpu) {
		if (target_list & (1<<cpu.cpu_id())) {
			cpu.gic().irq(irq).assert();
			cpu.recall();
		}
	});
};


void Cpu_base::initialize_boot(Vcpu_state &state, addr_t ip, addr_t dtb)
{
	state.reg(0, dtb);
	state.ip = ip;
}


void Cpu::setup_state(Vcpu_state &state)
{
	_sr_id_aa64isar0_el1.write(state.id_aa64isar0_el1);
	_sr_id_aa64isar1_el1.write(state.id_aa64isar1_el1);
	_sr_id_aa64mmfr0_el1.write(state.id_aa64mmfr0_el1);
	_sr_id_aa64mmfr1_el1.write(state.id_aa64mmfr1_el1);
	_sr_id_aa64mmfr2_el1.write(state.id_aa64mmfr2_el1);
	_sr_id_aa64pfr0_el1.write( _sr_id_aa64pfr0_el1.reset_value(
	                          state.id_aa64pfr0_el1));
	_sr_clidr_el1.write(state.clidr_el1);
	state.pstate     = 0b1111000101; /* el1 mode and IRQs disabled */
	state.vmpidr_el2 = cpu_id();
}


Cpu::Cpu(Vm              & vm,
         Vm_connection   & vm_session,
         Mmio_bus        & bus,
         Gic             & gic,
         Env             & env,
         Heap            & heap,
         Entrypoint      & ep,
         unsigned                  id)
: Cpu_base(vm, vm_session, bus, gic, env, heap, ep, id),
  _sr_id_aa64afr0_el1 (3, 0, 0, 5, 4, "ID_AA64AFR0_EL1",  false, 0x0, _reg_tree),
  _sr_id_aa64afr1_el1 (3, 0, 0, 5, 5, "ID_AA64AFR1_EL1",  false, 0x0, _reg_tree),
  _sr_id_aa64dfr0_el1 (3, 0, 0, 5, 0, "ID_AA64DFR0_EL1",  false, 0x6, _reg_tree),
  _sr_id_aa64dfr1_el1 (3, 0, 0, 5, 1, "ID_AA64DFR1_EL1",  false, 0x0, _reg_tree),
  _sr_id_aa64isar0_el1(3, 0, 0, 6, 0, "ID_AA64ISAR0_EL1", false, 0x0, _reg_tree),
  _sr_id_aa64isar1_el1(3, 0, 0, 6, 1, "ID_AA64ISAR1_EL1", false, 0x0, _reg_tree),
  _sr_id_aa64mmfr0_el1(3, 0, 0, 7, 0, "ID_AA64MMFR0_EL1", false, 0x0, _reg_tree),
  _sr_id_aa64mmfr1_el1(3, 0, 0, 7, 1, "ID_AA64MMFR1_EL1", false, 0x0, _reg_tree),
  _sr_id_aa64mmfr2_el1(3, 0, 0, 7, 2, "ID_AA64MMFR2_EL1", false, 0x0, _reg_tree),
  _sr_id_aa64pfr0_el1 (0x0, _reg_tree),
  _sr_id_aa64pfr1_el1 (3, 0, 0, 4, 1, "ID_AA64PFR1_EL1",  false, 0x0, _reg_tree),
  _sr_id_aa64zfr0_el1 (3, 0, 0, 4, 4, "ID_AA64ZFR0_EL1",  false, 0x0, _reg_tree),
  _sr_aidr_el1        (3, 0, 1, 0, 7, "AIDR_EL1",         false, 0x0, _reg_tree),
  _sr_revidr_el1      (3, 0, 0, 0, 6, "REVIDR_EL1",       false, 0x0, _reg_tree),
  _sr_clidr_el1       (3, 0, 1, 0, 1, "CLIDR_EL1",        false, 0x0, _reg_tree),
  _sr_csselr_el1      (3, 0, 2, 0, 0, "CSSELR_EL1",       true,  0x0, _reg_tree),
  _sr_ctr_el0         (_reg_tree),
  _sr_ccsidr_el1      (_sr_csselr_el1, *this, _reg_tree),
  _sr_pmuserenr_el0   (3, 9, 3, 14, 0, "PMUSEREN_EL0",    true,  0x0, _reg_tree),
  _sr_dbgbcr0         (2, 0, 0, 0, 5, "DBGBCR_EL1",       true,  0x0, _reg_tree),
  _sr_dbgbvr0         (2, 0, 0, 0, 4, "DBGBVR_EL1",       true,  0x0, _reg_tree),
  _sr_dbgwcr0         (2, 0, 0, 0, 7, "DBGWCR_EL1",       true,  0x0, _reg_tree),
  _sr_dbgwvr0         (2, 0, 0, 0, 6, "DBGWVR_EL1",       true,  0x0, _reg_tree),
  _sr_mdscr           (2, 0, 0, 2, 2, "MDSCR_EL1",        true,  0x0, _reg_tree),
  _sr_osdlr           (2, 1, 0, 3, 4, "OSDLR_EL1",        true,  0x0, _reg_tree),
  _sr_oslar           (2, 1, 0, 0, 4, "OSLAR_EL1",        true,  0x0, _reg_tree),
  _sr_sgi1r_el1       (_reg_tree, vm)
{ }
