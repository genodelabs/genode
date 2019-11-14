/*
 * \brief  VMM cpu object
 * \author Stefan Kalkowski
 * \date   2019-07-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
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

Genode::uint64_t Cpu_base::State::reg(unsigned idx) const
{
	if (idx > 30) return 0;
	return r[idx];
}


void Cpu_base::State::reg(unsigned idx, Genode::uint64_t v)
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


void Cpu_base::_handle_brk()
{
	Genode::uint64_t offset = 0x0;
	if (!(_state.pstate & 0b100)) {
		offset = 0x400;
	} else if (_state.pstate & 0b1) {
		offset = 0x200;
	}
	_state.esr_el1  = _state.esr_el2;
	_state.spsr_el1 = _state.pstate;
	_state.elr_el1  = _state.ip;
	_state.ip       = _state.vbar_el1 + offset;
	_state.pstate   = 0b1111000101;
}


void Cpu_base::handle_exception()
{
	/* check exception reason */
	switch (_state.exception_type) {
	case Cpu::NO_EXCEPTION:                 break;
	case Cpu::AARCH64_IRQ:  _handle_irq();  break;
	case Cpu::AARCH64_SYNC: _handle_sync(); break;
	default:
		throw Exception("Curious exception ",
		                _state.exception_type, " occured");
	}
	_state.exception_type = Cpu::NO_EXCEPTION;
}


void Cpu_base::dump()
{
	using namespace Genode;

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
		    Hex(_state.r[i], Hex::PREFIX, Hex::PAD));
	}
	log("  sp         = ", Hex(_state.sp,      Hex::PREFIX, Hex::PAD));
	log("  ip         = ", Hex(_state.ip,      Hex::PREFIX, Hex::PAD));
	log("  sp_el1     = ", Hex(_state.sp_el1,  Hex::PREFIX, Hex::PAD));
	log("  elr_el1    = ", Hex(_state.elr_el1, Hex::PREFIX, Hex::PAD));
	log("  pstate     = ", Hex(_state.pstate,  Hex::PREFIX, Hex::PAD));
	log("  exception  = ", _state.exception_type, " (",
	                       lambda(_state.exception_type), ")");
	log("  esr_el2    = ", Hex(_state.esr_el2, Hex::PREFIX, Hex::PAD));
	_timer.dump();
}


Genode::addr_t Cpu::Ccsidr::read() const
{
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

	unsigned level = Csselr::Level::get(csselr.read());
	bool     instr = Csselr::Instr::get(csselr.read());

	if (level > 6) {
		Genode::warning("Invalid Csselr value!");
		return INVALID;
	}

	unsigned ce = Clidr::level(level, state.clidr_el1);

	if (ce == Clidr::NO_CACHE ||
	    (ce == Clidr::DATA_CACHE_ONLY && instr)) {
		Genode::warning("Invalid Csselr value!");
		return INVALID;
	}

	if (ce == Clidr::INSTRUCTION_CACHE_ONLY ||
	    (ce == Clidr::SEPARATE_CACHE && instr)) {
		Genode::log("Return Ccsidr instr value ", state.ccsidr_inst_el1[level]);
		return state.ccsidr_inst_el1[level];
	}

	Genode::log("Return Ccsidr value ", state.ccsidr_data_el1[level]);
	return state.ccsidr_data_el1[level];
}


Genode::addr_t Cpu::Ctr_el0::read() const
{
	Genode::addr_t ret;
	asm volatile("mrs %0, ctr_el0" : "=r" (ret));
	return ret;
}


void Cpu::Icc_sgi1r_el1::write(Genode::addr_t v)
{

	unsigned target_list = v & 0xffff;
	unsigned irq         = (v >> 24) & 0xf;

	for (unsigned i = 0; i <= Vm::last_cpu(); i++) {
		if (target_list & (1<<i)) {
			vm.cpu(i, [&] (Cpu & cpu) {
				cpu.gic().irq(irq).assert();
				cpu.recall();
			});
		}
	}
};


void Cpu_base::initialize_boot(Genode::addr_t ip, Genode::addr_t dtb)
{
	state().reg(0, dtb);
	state().ip = ip;
}


Cpu::Cpu(Vm                      & vm,
         Genode::Vm_connection   & vm_session,
         Mmio_bus                & bus,
         Gic                     & gic,
         Genode::Env             & env,
         Genode::Heap            & heap,
         Genode::Entrypoint      & ep)
: Cpu_base(vm, vm_session, bus, gic, env, heap, ep),
  _sr_id_aa64afr0_el1 (3, 0, 0, 5, 4, "ID_AA64AFR0_EL1",  false, 0x0, _reg_tree),
  _sr_id_aa64afr1_el1 (3, 0, 0, 5, 5, "ID_AA64AFR1_EL1",  false, 0x0, _reg_tree),
  _sr_id_aa64dfr0_el1 (3, 0, 0, 5, 0, "ID_AA64DFR0_EL1",  false, 0x6, _reg_tree),
  _sr_id_aa64dfr1_el1 (3, 0, 0, 5, 1, "ID_AA64DFR1_EL1",  false, 0x0, _reg_tree),
  _sr_id_aa64isar0_el1(3, 0, 0, 6, 0, "ID_AA64ISAR0_EL1", false, _state.id_aa64isar0_el1, _reg_tree),
  _sr_id_aa64isar1_el1(3, 0, 0, 6, 1, "ID_AA64ISAR1_EL1", false, _state.id_aa64isar1_el1, _reg_tree),
  _sr_id_aa64mmfr0_el1(3, 0, 0, 7, 0, "ID_AA64MMFR0_EL1", false, _state.id_aa64mmfr0_el1, _reg_tree),
  _sr_id_aa64mmfr1_el1(3, 0, 0, 7, 1, "ID_AA64MMFR1_EL1", false, _state.id_aa64mmfr1_el1, _reg_tree),
  _sr_id_aa64mmfr2_el1(3, 0, 0, 7, 2, "ID_AA64MMFR2_EL1", false, _state.id_aa64mmfr2_el1, _reg_tree),
  _sr_id_aa64pfr0_el1 (_state.id_aa64pfr0_el1, _reg_tree),
  _sr_id_aa64pfr1_el1 (3, 0, 0, 4, 1, "ID_AA64PFR1_EL1",  false, 0x0, _reg_tree),
  _sr_id_aa64zfr0_el1 (3, 0, 0, 4, 4, "ID_AA64ZFR0_EL1",  false, 0x0, _reg_tree),
  _sr_aidr_el1        (3, 0, 1, 0, 7, "AIDR_EL1",         false, 0x0, _reg_tree),
  _sr_revidr_el1      (3, 0, 0, 0, 6, "REVIDR_EL1",       false, 0x0, _reg_tree),
  _sr_clidr_el1       (3, 0, 1, 0, 1, "CLIDR_EL1",        false, _state.clidr_el1,        _reg_tree),
  _sr_csselr_el1      (3, 0, 2, 0, 0, "CSSELR_EL1",       true,  0x0, _reg_tree),
  _sr_ctr_el0         (_reg_tree),
  _sr_ccsidr_el1      (_sr_csselr_el1, _state, _reg_tree),
  _sr_pmuserenr_el0   (3, 9, 3, 14, 0, "PMUSEREN_EL0",    true,  0x0, _reg_tree),
  _sr_dbgbcr0         (2, 0, 0, 0, 5, "DBGBCR_EL1",       true,  0x0, _reg_tree),
  _sr_dbgbvr0         (2, 0, 0, 0, 4, "DBGBVR_EL1",       true,  0x0, _reg_tree),
  _sr_dbgwcr0         (2, 0, 0, 0, 7, "DBGWCR_EL1",       true,  0x0, _reg_tree),
  _sr_dbgwvr0         (2, 0, 0, 0, 6, "DBGWVR_EL1",       true,  0x0, _reg_tree),
  _sr_mdscr           (2, 0, 0, 2, 2, "MDSCR_EL1",        true,  0x0, _reg_tree),
  _sr_osdlr           (2, 1, 0, 3, 4, "OSDLR_EL1",        true,  0x0, _reg_tree),
  _sr_oslar           (2, 1, 0, 0, 4, "OSLAR_EL1",        true,  0x0, _reg_tree),
  _sr_sgi1r_el1       (_reg_tree, vm)
{
	_state.pstate     = 0b1111000101; /* el1 mode and IRQs disabled */
	_state.vmpidr_el2 = cpu_id();
}
