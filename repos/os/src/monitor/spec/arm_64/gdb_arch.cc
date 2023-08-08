/*
 * \brief  Architecture-specific GDB protocol support
 * \author Christian Prochaska
 * \date   2023-07-31
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/endian.h>

/* monitor includes */
#include <gdb_arch.h>
#include <monitored_thread.h>

using namespace Monitor;


/* BRK */
char const *Monitor::Gdb::breakpoint_instruction()     { return "\x20\x00\x20\xd4"; }
size_t      Monitor::Gdb::breakpoint_instruction_len() { return 4; }


void Monitor::Gdb::print_registers(Output &out, Cpu_state const &cpu)
{
	for (addr_t r : cpu.r)
		print(out, Gdb_hex(host_to_big_endian(r)));

	print(out, Gdb_hex(host_to_big_endian(cpu.sp)));
	print(out, Gdb_hex(host_to_big_endian(cpu.ip)));
}


void Monitor::Gdb::parse_registers(Const_byte_range_ptr const &in, Cpu_state &cpu)
{
	for (size_t i = 0; i < 33; i++) {
		with_skipped_bytes(in, i * sizeof(addr_t) * 2,
		                   [&] (Const_byte_range_ptr const &in) {
			with_max_bytes(in, sizeof(addr_t) * 2, [&] (Const_byte_range_ptr const &in) {
				char null_terminated[sizeof(addr_t) * 2 + 1] { };
				memcpy(null_terminated, in.start,
				       min(sizeof(null_terminated) - 1, in.num_bytes));
				addr_t value = 0;
				ascii_to_unsigned(null_terminated, value, 16);
				if (i < 31) {
					cpu.r[i] = big_endian_to_host(value);
				} else if (i == 31) {
					cpu.sp =  big_endian_to_host(value);
				} else if (i == 32) {
					cpu.ip =  big_endian_to_host(value);
				}
			});
		});
	}
}


void Monitor::Monitored_thread::_handle_exception()
{
	stop_state = Stop_state::STOPPED_REPLY_PENDING;

	Thread_state thread_state = _real.call<Rpc_get_state>();

	if (_wait) {
		_wait = false;
		_thread_monitor.remove_initial_breakpoint(_pd, _first_instruction_addr,
			                                      _original_first_instruction);
		stop_reply_signal = Stop_reply_signal::STOP;
	} else {
		switch(thread_state.ec) {
		case Cpu_state::Cpu_exception::SOFTWARE_STEP:
			stop_reply_signal = Stop_reply_signal::TRAP;
			break;
		case Cpu_state::Cpu_exception::BREAKPOINT:
			stop_reply_signal = Stop_reply_signal::TRAP;
			break;
		default:
			stop_reply_signal = Stop_reply_signal::TRAP;
		}
	}

	_thread_monitor.thread_stopped(_pd, *this);
}
