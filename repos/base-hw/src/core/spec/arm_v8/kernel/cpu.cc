/*
 * \brief   Kernel cpu driver implementations specific to ARMv8
 * \author  Stefan Kalkowski
 * \date    2019-05-11
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <hw/memory_consts.h>


void Kernel::Cpu::_arch_init()
{
	/* enable timer interrupt */
	_pic.unmask(_timer.interrupt_id(), id());
}


[[noreturn]] void Kernel::Cpu::panic(Genode::Cpu_state &state)
{
	using namespace Genode;

	Core::Cpu::Context &context = static_cast<Core::Cpu::Context&>(state);

	const char *reason = "unknown";

	switch (context.exception_type) {
	case Cpu::SYNC_LEVEL_EL0:         [[fallthrough]];
	case Cpu::SYNC_LEVEL_EL1:         [[fallthrough]];
	case Cpu::SYNC_LEVEL_EL1_EXC_MODE:
		using Ec = Cpu::Esr::Ec;
		switch (Ec::get(state.esr_el1)) {
		case Ec::INST_ABORT_SAME_LEVEL:   [[fallthrough]];
		case Ec::DATA_ABORT_SAME_LEVEL:   [[fallthrough]];
		case Ec::INST_ABORT_LOW_LEVEL:    [[fallthrough]];
		case Ec::DATA_ABORT_LOW_LEVEL:    reason = "page-fault";  break;
		case Ec::SVC:                     reason = "system-call"; break;
		case Ec::SOFTWARE_STEP_LOW_LEVEL: [[fallthrough]];
		case Ec::BRK:                     reason = "debug";       break;
		default: ;
		}
		break;
	case Cpu::IRQ_LEVEL_EL0:          [[fallthrough]];
	case Cpu::IRQ_LEVEL_EL1:          [[fallthrough]];
	case Cpu::IRQ_LEVEL_EL1_EXC_MODE: [[fallthrough]];
	case Cpu::FIQ_LEVEL_EL0:          [[fallthrough]];
	case Cpu::FIQ_LEVEL_EL1:          [[fallthrough]];
	case Cpu::FIQ_LEVEL_EL1_EXC_MODE: reason = "interrupt "; break;
	case Cpu::RESET:                  reason = "reset ";     break;
	default: ;
	};

	log("");
	log("Kernel panic on CPU ", Cpu::executing_id());
	log("Exception reason is ", reason);
	log("");
	log("Register dump:");
	for (unsigned i = 0; i < 31; i++)
		log("r", i, i<10 ? " " : "", "     = ", Hex(state.r[i]));
	log("sp      = ", Hex(state.sp));
	log("ip      = ", Hex(state.ip));
	log("esr_el1 = ", Hex(state.esr_el1));
	log("far_el1 = ", Hex(Far_el1::read()), " (fault-address if page-fault)");
	log("");
	log("Backtrace:");

	Const_byte_range_ptr const stack {
		(char const*)stack_base(), Hw::Mm::KERNEL_STACK_SIZE };
	context.for_each_return_address(stack, [&] (void **p) { log(*p); });

	while (true) asm volatile("wfi");
}
