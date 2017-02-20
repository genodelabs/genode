/*
 * \brief  Virtual Machine Monitor VM definition
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>

/* local includes */
#include <vm_base.h>
#include <mmu.h>

using namespace Genode;


void Vm_base::_load_kernel()
{
	Attached_rom_dataspace kernel(_env, _kernel.string());
	memcpy((void*)(_ram.local() + _kernel_off),
	       kernel.local_addr<void>(), kernel.size());
	_state.ip = _ram.base() + _kernel_off;
}


Vm_base::Vm_base(Env                &env,
                 Kernel_name  const &kernel,
                 Command_line const &cmdline,
                 addr_t              ram_base,
                 size_t              ram_size,
                 off_t               kernel_off,
                 Machine_type        machine,
                 Board_revision      board)
:
	_env(env), _kernel(kernel), _cmdline(cmdline), _kernel_off(kernel_off),
	_machine(machine), _board(board), _ram(env, ram_base, ram_size)
{
	_state.irq_injection = 0;
}

void Vm_base::start()
{
	memset((void*)&_state, 0, sizeof(Vm_state));
	_load_kernel();
	_load_kernel_surroundings();
	_state.cpsr = 0x93; /* SVC mode and IRQs disabled */
	_state.r0   = 0;
	_state.r1   = _machine.value;
	_state.r2   = _ram.base() + _board_info_offset();
}


void Vm_base::inject_irq(unsigned irq)
{
	if (_state.irq_injection) { throw Inject_irq_failed(); }
	_state.irq_injection = irq;
}



void Vm_base::dump()
{
	char const *mod[] = { "und", "svc", "abt", "irq", "fiq" };
	char const *exc[] = { "invalid", "reset", "undefined", "smc",
	                      "pf_abort", "data_abort", "irq", "fiq" };

	auto log_adr_reg = [&] (char const *reg, addr_t val) {
		log("  ", reg, "      = ", Hex(val, Hex::PREFIX, Hex::PAD), " ",
		    Hex(va_to_pa(val), Hex::PREFIX, Hex::PAD)); };

	auto log_mod_reg = [&] (char const *reg, addr_t val, char const *mod) {
		log("  ", reg, "_", mod, "  = ", Hex(val, Hex::PREFIX, Hex::PAD), " ",
		    Hex(va_to_pa(val), Hex::PREFIX, Hex::PAD)); };

	log("Cpu state:");
	log("  Register     Virt       Phys");
	log("------------------------------------");
	log_adr_reg("r0   ", _state.r0);
	log_adr_reg("r1   ", _state.r1);
	log_adr_reg("r2   ", _state.r2);
	log_adr_reg("r3   ", _state.r3);
	log_adr_reg("r4   ", _state.r4);
	log_adr_reg("r5   ", _state.r5);
	log_adr_reg("r6   ", _state.r6);
	log_adr_reg("r7   ", _state.r7);
	log_adr_reg("r8   ", _state.r8);
	log_adr_reg("r9   ", _state.r9);
	log_adr_reg("r10  ", _state.r10);
	log_adr_reg("r11  ", _state.r11);
	log_adr_reg("r12  ", _state.r12);
	log_adr_reg("sp   ", _state.sp);
	log_adr_reg("lr   ", _state.lr);
	log_adr_reg("ip   ", _state.ip);
	log_adr_reg("cpsr ", _state.cpsr);
	for (unsigned i = 0; i < Vm_state::Mode_state::MAX; i++) {
		log_mod_reg("sp   ", _state.mode[i].sp,   mod[i]);
		log_mod_reg("lr   ", _state.mode[i].lr,   mod[i]);
		log_mod_reg("spsr ", _state.mode[i].spsr, mod[i]);
	}
	log("  ttbr0      = ", Hex(_state.ttbr[0], Hex::PREFIX, Hex::PAD));
	log("  ttbr1      = ", Hex(_state.ttbr[1], Hex::PREFIX, Hex::PAD));
	log("  ttbrc      = ", Hex(_state.ttbrc  , Hex::PREFIX, Hex::PAD));
	log_adr_reg("dfar ",  _state.dfar);
	log("  exception  = ", exc[_state.cpu_exception]);
}


void Vm_base::smc_ret(addr_t const ret_0, addr_t const ret_1)
{
	_state.r0 = ret_0;
	_state.r1 = ret_1;
}


addr_t Vm_base::va_to_pa(addr_t va)
{
	try {
		Mmu mmu(_state, _ram);
		return mmu.phys_addr(va);
	}
	catch(Ram::Invalid_addr) { }
	return 0;
}
