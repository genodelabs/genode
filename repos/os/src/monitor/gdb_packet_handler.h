/*
 * \brief  GDB packet handler
 * \author Norman Feske
 * \date   2023-05-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GDB_PACKET_HANDLER_H_
#define _GDB_PACKET_HANDLER_H_

#include <monitor/gdb_packet.h>

/* local includes */
#include <gdb_command.h>

namespace Monitor { namespace Gdb { struct Packet_handler; } }


struct Monitor::Gdb::Packet_handler
{
	using Gdb_packet = Genode::Gdb_packet<GDB_PACKET_MAX_SIZE>;

	Gdb_packet _packet { };

	bool execute(State                      &state,
	             Commands             const &commands,
	             Const_byte_range_ptr const &input,
	             Output                     &output)
	{
		bool progress = false;
		for (unsigned i = 0; i < input.num_bytes; i++) {

			using Result = Gdb_packet::Append_result;

			switch (_packet.append(input.start[i])) {

			case Result::COMPLETE:
				_packet.with_command([&] (Const_byte_range_ptr const &bytes) {
					bool handled = false;
					commands.for_each([&] (Command const &command) {
						command.with_args(bytes, [&] (Const_byte_range_ptr const &args) {
							print(output, "+"); /* ack */
							command.execute(state, args, output);
							handled = true; }); });

					if (!handled)
						warning("unhandled GDB command: ",
						        Cstring(bytes.start, bytes.num_bytes));
				});
				_packet.reset();
				break;

			case Result::OVERFLOW:
				error("received unexpectedly large GDB command");
				_packet.reset();
				break;

			case Result::CORRUPT:
				error("received GDB command that could not be parsed");
				_packet.reset();
				break;

			case Result::OK:
				break;
			}
			progress = true;
		}
		return progress;
	}
};

#endif /* _GDB_PACKET_HANDLER_H_ */
