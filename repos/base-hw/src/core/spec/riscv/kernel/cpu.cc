/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
 * \author  Sebastian Sumpf
 * \date    2015-06-02
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <hw/memory_map.h>
#include <hw/memory_consts.h>


void Kernel::Cpu::_arch_init() {
	Stvec::write(Hw::Mm::supervisor_exception_vector().base); }


[[noreturn]] void Kernel::Cpu::panic(Genode::Cpu_state &state)
{
	using namespace Genode;
	using Cs = Genode::Cpu_state;

	Core::Cpu::Context &context = static_cast<Core::Cpu::Context&>(state);

	const char *reason = "unknown";

	if (context.is_irq()) reason = "interrupt";

	switch(state.cpu_exception) {
	case Cs::ECALL_FROM_SUPERVISOR:    [[fallthrough]];
	case Cs::ECALL_FROM_USER:          reason = "system-call"; break;
	case Cs::INSTRUCTION_PAGE_FAULT:   [[fallthrough]];
	case Cs::STORE_PAGE_FAULT:         [[fallthrough]];
	case Cs::LOAD_PAGE_FAULT:          [[fallthrough]];
	case Cs::INSTRUCTION_ACCESS_FAULT: [[fallthrough]];
	case Cs::LOAD_ACCESS_FAULT:        [[fallthrough]];
	case Cs::STORE_ACCESS_FAULT:       reason = "page-fault"; break;
	case Cs::INSTRUCTION_ILLEGAL:      reason = "undefined-instruction"; break;
	case Cs::BREAKPOINT:               reason = "debug"; break;
	case Cs::RESET:                    reason = "reset"; break;
	default: ;
	}

	log("");
	log("Kernel panic on CPU ", Cpu::executing_id());
	log("Exception reason is ", reason);
	log("");
	log("Register dump:");
	log("ip   = ", Hex(state.ip));
	log("ra   = ", Hex(state.ra));
	log("sp   = ", Hex(state.sp));
	log("gp   = ", Hex(state.gp));
	log("tp   = ", Hex(state.tp));
	log("t0   = ", Hex(state.t0));
	log("t1   = ", Hex(state.t1));
	log("t2   = ", Hex(state.t2));
	log("s0   = ", Hex(state.s0));
	log("s1   = ", Hex(state.s1));
	log("a0   = ", Hex(state.a0));
	log("a1   = ", Hex(state.a1));
	log("a2   = ", Hex(state.a2));
	log("a3   = ", Hex(state.a3));
	log("a4   = ", Hex(state.a4));
	log("a5   = ", Hex(state.a5));
	log("a6   = ", Hex(state.a6));
	log("a7   = ", Hex(state.a7));
	log("s2   = ", Hex(state.s2));
	log("s3   = ", Hex(state.s3));
	log("s4   = ", Hex(state.s4));
	log("s5   = ", Hex(state.s5));
	log("s6   = ", Hex(state.s6));
	log("s7   = ", Hex(state.s7));
	log("s8   = ", Hex(state.s8));
	log("s9   = ", Hex(state.s9));
	log("s10  = ", Hex(state.s10));
	log("s11  = ", Hex(state.s11));
	log("t3   = ", Hex(state.t3));
	log("t4   = ", Hex(state.t4));
	log("t5   = ", Hex(state.t5));
	log("t6   = ", Hex(state.t6));
	log("");
	log("Backtrace:");

	Const_byte_range_ptr const stack {
		(char const*)stack_base(), Hw::Mm::KERNEL_STACK_SIZE };
	context.for_each_return_address(stack, [&] (void **p) { log(*p); });

	while (true) asm volatile("wfi");
}
