/*
 * \brief  Utility for interacting with the monitor runtime
 * \author Norman Feske
 * \date   2023-06-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MONITOR_CONTROLLER_H_
#define _MONITOR_CONTROLLER_H_

/* Genode includes */
#include <terminal_session/connection.h>
#include <monitor/gdb_packet.h>
#include <monitor/output.h>
#include <monitor/string.h>

namespace Monitor {

	using namespace Genode;

	class Controller;
}


/**
 * Utility for the synchronous interaction with a GDB stub over a terminal
 *
 * Note that requests and responses are limited to GDB_PACKET_MAX_SIZE.
 */
class Monitor::Controller : Noncopyable
{
	private:

		static constexpr uint16_t GDB_PACKET_MAX_SIZE = 16*1024;

		Env &_env;

		Terminal::Connection _terminal { _env };

		char _buffer[GDB_PACKET_MAX_SIZE] { };

		Io_signal_handler<Controller> _terminal_read_avail_handler {
			_env.ep(), *this, &Controller::_handle_terminal_read_avail };

		void _handle_terminal_read_avail() { }

		void _request(auto &&... args)
		{
			struct Terminal_output
			{
				struct Write_fn
				{
					Terminal::Connection &_terminal;
					void operator () (char const *str) {
						_terminal.write(str, strlen(str)); }
				} _write_fn;

				Buffered_output<1024, Write_fn> buffered { _write_fn };
			};

			Terminal_output output { ._write_fn { _terminal } };

			Gdb_checksummed_output checksummed_output { output.buffered, false };

			print(checksummed_output, args...);
		};

		void _with_response(auto const &fn)
		{
			using Gdb_packet = Genode::Gdb_packet<GDB_PACKET_MAX_SIZE>;

			Gdb_packet _packet { };

			while (!_packet.complete()) {

				size_t const read_num_bytes = _terminal.read(_buffer, sizeof(_buffer));

				for (unsigned i = 0; i < read_num_bytes; i++) {

					using Result = Gdb_packet::Append_result;

					switch (_packet.append(_buffer[i])) {

					case Result::COMPLETE:
						fn(Const_byte_range_ptr(_packet.buf, _packet.cursor));
						_terminal.write("+", 1); /* ack */
						return;

					case Result::OVERFLOW:
						error("received unexpectedly large GDB response");
						break;

					case Result::CORRUPT:
						error("received GDB response that could not be parsed");
						break;

					case Result::OK:
						break;
					}
				}
				if (read_num_bytes == 0)
					_env.ep().wait_and_dispatch_one_io_signal();
			}
		}

		bool _response_ok()
		{
			bool ok = false;
			_with_response([&] (Const_byte_range_ptr const &response) {
				if ((response.num_bytes == 2) &&
				    (response.start[0] == 'O') &&
				    (response.start[1] == 'K'))
				    ok = true;
			});
			return ok;
		}

		/* convert binary buffer to ASCII hex characters */
		struct Gdb_hex_buffer
		{
			Const_byte_range_ptr const &src;

			Gdb_hex_buffer(Const_byte_range_ptr const &src)
			: src(src) { }

			void print(Output &output) const
			{
				for (size_t i = 0; i < src.num_bytes; i++)
					Genode::print(output, Gdb_hex(src.start[i]));
			}
		};

	public:

		Controller(Env &env) : _env(env)
		{
			_terminal.read_avail_sigh(_terminal_read_avail_handler);
		}

		struct Thread_info
		{
			using Name = String<64>;
			Name name;

			unsigned pid;  /* inferior ID */
			unsigned tid;  /* thread ID */

			static Thread_info from_xml(Xml_node const &node)
			{
				using Id = String<16>;
				Id const id = node.attribute_value("id", Id());
				char const *s = id.string();

				/* parse GDB's thread ID format, e.g., 'p1.2' */
				unsigned pid = 0, tid = 0;
				if (*s == 'p') s++;
				s = s + ascii_to(s, pid);
				if (*s == '.') s++;
				ascii_to(s, tid);

				return { .name = node.attribute_value("name", Name()),
				         .pid  = pid,
				         .tid  = tid };
			}
		};

		/**
		 * Call 'fn' for each thread with the 'Thread_info' as argument
		 */
		void for_each_thread(auto const &fn)
		{
			_request("qXfer:threads:read::0,1000");

			_with_response([&] (Const_byte_range_ptr const &response) {
				with_skipped_prefix(response, "l", [&] (Const_byte_range_ptr const &payload) {
					Xml_node node(payload.start, payload.num_bytes);
					node.for_each_sub_node("thread", [&] (Xml_node const &thread) {
						fn(Thread_info::from_xml(thread)); }); }); });
		}

		/**
		 * Read memory 'at' from current inferior
		 */
		size_t read_memory(addr_t at, Byte_range_ptr const &dst)
		{
			/*
			 * Memory dump is in hex format (two digits per byte), also
			 * account for the protocol overhead (checksum, prefix).
			 */
			size_t num_bytes = min(dst.num_bytes, size_t(GDB_PACKET_MAX_SIZE)/2 - 16);

			_request("m", Gdb_hex(at), ",", Gdb_hex(num_bytes));

			size_t read_bytes = 0;
			_with_response([&] (Const_byte_range_ptr const &response) {

				read_bytes = min(response.num_bytes/2, dst.num_bytes);

				/* convert ASCII hex to bytes */
				char const *s = response.start;
				uint8_t    *d = (uint8_t *)dst.start;
				for (unsigned i = 0; i < read_bytes; i++) {
					char const hi = *s++;
					char const lo = *s++;
					*d++ = uint8_t(digit(hi, true) << 4)
					     | uint8_t(digit(lo, true));
				}
			});
			return read_bytes;
		}

		/**
		 * Write memory 'at' of current inferior
		 */
		bool write_memory(addr_t at, Const_byte_range_ptr const &src)
		{
			_request("M", Gdb_hex(at), ",", Gdb_hex(src.num_bytes), ":",
			         Gdb_hex_buffer(src));

			return _response_ok();
		}
};

#endif /* _MONITOR_CONTROLLER_H_ */
