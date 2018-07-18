/*
 * \brief  Character printing utilities
 * \author Emery Hemingway
 * \date   2018-07-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL__PRINT_H_
#define _TERMINAL__PRINT_H_

#include <log_session/connection.h>
#include <base/output.h>

namespace Terminal {

	class Log_buffer : public Genode::Output
	{
		private:

			enum { BUF_SIZE = Genode::Log_session::MAX_STRING_LEN };

			char _buf[BUF_SIZE];
			unsigned _num_chars = 0;

		public:

			Log_buffer() { }

			void flush_ok()
			{
				log(Genode::Cstring(_buf, _num_chars));
				_num_chars = 0;
			}

			void flush_warning()
			{
				warning(Genode::Cstring(_buf, _num_chars));
				_num_chars = 0;
			}

			void flush_error()
			{
				error(Genode::Cstring(_buf, _num_chars));
				_num_chars = 0;
			}

			void out_char(char c) override
			{
				_buf[_num_chars++] = c;
				if (_num_chars >= sizeof(_buf))
					flush_ok();
			}

			template <typename... ARGS>
			void print(ARGS &&... args) {
				Output::out_args(*this, args...); }
	};

	struct Ascii
	{
		unsigned char const _c;

		template <typename T>
		explicit Ascii(T c) : _c(c) { }

		void print(Genode::Output &out) const
		{
			switch (_c) {
			case 000: out.out_string("NUL"); break;
			case 001: out.out_string("SOH"); break;
			case 002: out.out_string("STX"); break;
			case 003: out.out_string("ETX"); break;
			case 004: out.out_string("EOT"); break;
			case 005: out.out_string("ENQ"); break;
			case 006: out.out_string("ACK"); break;
			case 007: out.out_string("BEL"); break;
			case 010: out.out_string("BS");  break;
			case 011: out.out_string("HT");  break;
			case 012: out.out_string("LF");  break;
			case 013: out.out_string("VT");  break;
			case 014: out.out_string("FF");  break;
			case 015: out.out_string("CR");  break;
			case 016: out.out_string("SO");  break;
			case 017: out.out_string("SI");  break;
			case 020: out.out_string("DLE"); break;
			case 021: out.out_string("DC1"); break;
			case 022: out.out_string("DC2"); break;
			case 023: out.out_string("DC3"); break;
			case 024: out.out_string("DC4"); break;
			case 025: out.out_string("NAK"); break;
			case 026: out.out_string("SYN"); break;
			case 027: out.out_string("ETB"); break;
			case 030: out.out_string("CAN"); break;
			case 031: out.out_string("EM");  break;
			case 032: out.out_string("SUB"); break;
			case 033: out.out_string("ESC"); break;
			case 034: out.out_string("FS");  break;
			case 035: out.out_string("GS");  break;
			case 036: out.out_string("RS");  break;
			case 037: out.out_string("US");  break;
			case 040: out.out_string("SPACE"); break;
			case 0177: out.out_string("DEL");  break;
			default:
				if (_c & 0x80)
					Genode::Hex(_c).print(out);
				else
					out.out_char(_c);
				break;
			}
		}
	};

	struct Ecma
	{
		unsigned char const _c;

		template <typename T>
		explicit Ecma(T c) : _c(c) { }

		void print(Genode::Output &out) const
		{
			Ascii(_c).print(out);
			out.out_char('(');

			Genode::print(out, (_c)/160,    (_c>>4)%10,
			              "/", (_c&0xf)/10, (_c&0xf)%10);

			out.out_char(')');
		}
	};

}

#endif
