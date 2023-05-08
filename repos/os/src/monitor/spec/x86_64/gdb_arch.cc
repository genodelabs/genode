/*
 * \brief  Architecture-specific GDB protocol support
 * \author Norman Feske
 * \date   2023-05-15
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/endian.h>
#include <gdb_arch.h>

using namespace Monitor;


void Monitor::Gdb::print_registers(Output &out, Cpu_state const &cpu)
{
	uint64_t const values_64bit[] = {
		cpu.rax, cpu.rbx, cpu.rcx, cpu.rdx, cpu.rsi, cpu.rdi, cpu.rbp, cpu.sp,
		cpu.r8,  cpu.r9,  cpu.r10, cpu.r11, cpu.r12, cpu.r13, cpu.r14, cpu.r15,
		cpu.ip };

	for (uint64_t value : values_64bit)
		print(out, Gdb_hex(host_to_big_endian(value)));

	uint32_t const values_32bit[] = {
		uint32_t(cpu.eflags), uint32_t(cpu.cs), uint32_t(cpu.ss),
		0 /* es */, 0 /* fs */, /* gs */ };

	for (uint32_t value : values_32bit)
		print(out, Gdb_hex(host_to_big_endian(value)));
}

