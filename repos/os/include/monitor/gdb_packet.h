/*
 * \brief  GDB packet parser
 * \author Norman Feske
 * \date   2023-05-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MONITOR__GDB_PACKET_H_
#define _MONITOR__GDB_PACKET_H_

#include <util/string.h>

namespace Genode { template <size_t> struct Gdb_packet; }


template <Genode::size_t MAX_SIZE>
struct Genode::Gdb_packet
{
	enum class State {
		IDLE, INCOMPLETE,
		EXPECT_CHECKSUM1, EXPECT_CHECKSUM2,
		COMPLETE, CORRUPT
	};

	State state { State::IDLE };

	unsigned cursor = 0;

	struct Checksum
	{
		uint8_t accumulated;
		uint8_t expected;

		bool matches() const { return (accumulated == expected); }
	};

	Checksum checksum { };

	char buf[MAX_SIZE] { };

	void reset()
	{
		state    = State::IDLE;
		cursor   = 0;
		checksum = { };
	}

	enum class Append_result { OK, COMPLETE, OVERFLOW, CORRUPT };

	Append_result append(char const c)
	{
		if (cursor >= sizeof(buf))
			return Append_result::OVERFLOW;

		auto interpret = [&]
		{
			auto is_hex_digit = [] (char c) { return is_digit(c, true); };
			auto hex_digit    = [] (char c) { return digit   (c, true); };

			if (state == State::EXPECT_CHECKSUM1 || state == State::EXPECT_CHECKSUM2)
				if (!is_hex_digit(c))
					return State::CORRUPT;

			switch (state) {

			case State::IDLE:
				return (c == '$') ? State::INCOMPLETE
				                  : State::IDLE;
			case State::INCOMPLETE:
				return (c == '#') ? State::EXPECT_CHECKSUM1
				                  : State::INCOMPLETE;
			case State::EXPECT_CHECKSUM1:
				checksum.expected = uint8_t(hex_digit(c) << 4u);
				return State::EXPECT_CHECKSUM2;

			case State::EXPECT_CHECKSUM2:
				checksum.expected |= uint8_t(hex_digit(c));
				return checksum.matches() ? State::COMPLETE
				                          : State::CORRUPT;
			case State::COMPLETE:
			case State::CORRUPT:
				break;     /* expect call of 'reset' */
			};
			return state;
		};

		State const orig_state = state;

		state = interpret();

		/* capture only the command payload between '$' and '#' */
		if ((orig_state == State::INCOMPLETE) && (state == State::INCOMPLETE)) {
			buf[cursor++] = c;
			checksum.accumulated = uint8_t(checksum.accumulated + c);
		}

		return (state == State::COMPLETE) ? Append_result::COMPLETE :
		       (state == State::CORRUPT)  ? Append_result::CORRUPT  :
		                                    Append_result::OK;
	}

	bool complete() const { return state == State::COMPLETE; }

	void with_command(auto const &fn) const
	{
		if (complete())
			fn(Const_byte_range_ptr { buf, cursor });
	}
};

#endif /* _MONITOR__GDB_PACKET_H_ */
