/*
 * \brief   Kernel cpu driver implementations specific to ARM
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <kernel/perf_counter.h>
#include <hw/memory_consts.h>


void Kernel::Cpu::_arch_init()
{
	enable_performance_counter();

	/* enable timer interrupt */
	_pic.unmask(_timer.interrupt_id(), id());
}


[[noreturn]] void Kernel::Cpu::panic(Genode::Cpu_state &state)
{
	using namespace Genode;
	using Cs = Genode::Cpu_state;

	const char *reason = "unknown";

	switch (state.cpu_exception) {
	case Cs::PREFETCH_ABORT:         [[fallthrough]];
	case Cs::DATA_ABORT:             reason = "page-fault";            break;
	case Cs::UNDEFINED_INSTRUCTION:  reason = "undefined instruction"; break;
	case Cs::SUPERVISOR_CALL:        reason = "system-call";           break;
	case Cs::FAST_INTERRUPT_REQUEST: [[fallthrough]];
	case Cs::INTERRUPT_REQUEST:      reason = "interrupt ";            break;
	case Cs::RESET            :      reason = "reset ";                break;
	default: ;
	};

	log("");
	log("Kernel panic on CPU ", Cpu::executing_id());
	log("Exception reason is ", reason);
	log("");
	log("Register dump:");
	log("r0     = ", Hex(state.r0));
	log("r1     = ", Hex(state.r1));
	log("r2     = ", Hex(state.r2));
	log("r3     = ", Hex(state.r3));
	log("r4     = ", Hex(state.r4));
	log("r5     = ", Hex(state.r5));
	log("r6     = ", Hex(state.r6));
	log("r7     = ", Hex(state.r7));
	log("r8     = ", Hex(state.r8));
	log("r9     = ", Hex(state.r9));
	log("r10    = ", Hex(state.r10));
	log("r11    = ", Hex(state.r11));
	log("r12    = ", Hex(state.r12));
	log("sp     = ", Hex(state.sp));
	log("lr     = ", Hex(state.lr));
	log("ip     = ", Hex(state.ip));
	log("cpsr   = ", Hex(state.cpsr));
	log("");
	log("Backtrace:");

	Board::Cpu::Context &context = static_cast<Board::Cpu::Context&>(state);
	Const_byte_range_ptr const stack {
		(char const*)stack_base(), Hw::Mm::KERNEL_STACK_SIZE };
	context.for_each_return_address(stack, [&] (void **p) { log(*p); });

	while (true) asm volatile("wfi");
}
