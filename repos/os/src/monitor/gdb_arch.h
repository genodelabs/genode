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

#ifndef _GDB_ARCH_H_
#define _GDB_ARCH_H_

#include <cpu/cpu_state.h>
#include <gdb_response.h>
#include <types.h>

namespace Monitor { namespace Gdb {

	/* needed for the array size of the saved original instruction */
	static constexpr size_t max_breakpoint_instruction_len = 8;

	char const *breakpoint_instruction();
	size_t breakpoint_instruction_len();

	void print_registers(Output &out, Cpu_state const &cpu);
	void parse_registers(Const_byte_range_ptr const &in, Cpu_state &cpu);
} }

#endif /* _GDB_ARCH_H_ */
