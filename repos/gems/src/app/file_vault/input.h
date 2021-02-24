/*
 * \brief  Text buffer for a passphrase
 * \author Norman Feske
 * \author Martin Stein
 * \date   2021-03-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INPUT_H_
#define _INPUT_H_

/* Genode includes */
#include <base/output.h>
#include <util/utf8.h>
#include <base/buffered_output.h>

/* local includes */
#include <types.h>

namespace File_vault {

	class Input_single_line;
	class Input_passphrase;
	class Input_number_of_bytes;
	class Input_number_of_blocks;
}


class File_vault::Input_single_line
{
	public:

		enum { MAX_LENGTH = 64 };

	protected:

		Codepoint _characters[MAX_LENGTH] { };

		unsigned _length = 0;

		void _print_characters(Output &out) const
		{
			/*
			 * FIXME This was copied from gems/src/server/terminal/main.cc
			 */

			struct Utf8 { char b0, b1, b2, b3, b4; };

			auto utf8_from_codepoint = [] (unsigned c) {

				/* extract 'n' bits 'at' bit position of value 'c' */
				auto bits = [c] (unsigned at, unsigned n) {
					return (c >> at) & ((1 << n) - 1); };

				return (c < 2<<7)  ? Utf8 { char(bits( 0, 7)), 0, 0, 0, 0 }
				     : (c < 2<<11) ? Utf8 { char(bits( 6, 5) | 0xc0),
				                            char(bits( 0, 6) | 0x80), 0, 0, 0 }
				     : (c < 2<<16) ? Utf8 { char(bits(12, 4) | 0xe0),
				                            char(bits( 6, 6) | 0x80),
				                            char(bits( 0, 6) | 0x80), 0, 0 }
				     : (c < 2<<21) ? Utf8 { char(bits(18, 3) | 0xf0),
				                            char(bits(12, 6) | 0x80),
				                            char(bits( 6, 6) | 0x80),
				                            char(bits( 0, 6) | 0x80), 0 }
				     : Utf8 { };
			};

			for (unsigned i = 0; i < _length; i++) {

				Utf8 const utf8 = utf8_from_codepoint(_characters[i].value);

				auto _print = [&] (char c) {
					if (c)
						Genode::print(out, Char(c)); };

				_print(utf8.b0); _print(utf8.b1); _print(utf8.b2);
				_print(utf8.b3); _print(utf8.b4);
			}
		}

	public:

		void append_character(Codepoint c)
		{
			if (_length < MAX_LENGTH) {
				_characters[_length] = c;
				_length++;
			}
		}

		void remove_last_character()
		{
			if (_length > 0) {
				_length--;
				_characters[_length].value = 0;
			}
		}

		bool equals(Input_single_line const &other) const
		{
			if (other._length != _length) {
				return false;
			}
			if (memcmp(other._characters, _characters, _length) != 0) {
				return false;
			}
			return true;
		}

		unsigned length() const { return _length; }
};



class File_vault::Input_passphrase : public Input_single_line
{
	private:

		bool _hide { true };

		void _print_bullets(Output &out) const
		{
			char const bullet_utf8[4] {
				(char)0xe2, (char)0x80, (char)0xa2, 0 };

			for (unsigned i = 0; i < _length; i++)
				Genode::print(out, bullet_utf8);
		}

	public:

		bool suitable() const
		{
			return _length >= 8;
		}

		char const *not_suitable_text() const
		{
			return "Must have at least 8 characters!";
		}

		void print(Output &out) const
		{
			if (_hide) {
				_print_bullets(out);
			} else {
				_print_characters(out);
			}
		}

		void hide(bool value)
		{
			_hide = value;
		}

		bool hide() const
		{
			return _hide;
		}

		bool appendable_character(Codepoint code)
		{
			if (!code.valid()) {
				return false;
			}
			bool const is_printable {
				code.value >= 0x20 && code.value < 0xf000 };

			return is_printable;
		}

		String<MAX_LENGTH * 3> plaintext() const
		{
			String<MAX_LENGTH * 3> result { };

			auto write = [&] (char const *str)
			{
				result = Cstring(str, strlen(str));
			};
			Buffered_output<MAX_LENGTH * 3, decltype(write)> output(write);

			_print_characters(output);
			return result;
		}
};


class File_vault::Input_number_of_bytes : public Input_single_line
{
	public:

		void print(Output &out) const
		{
			_print_characters(out);
		}

		size_t value() const
		{
			String<32> const str { *this };
			Number_of_bytes result { 0 };
			ascii_to(str.string(), result);
			return result;
		}

		bool appendable_character(Codepoint code)
		{
			if (!code.valid()) {
				return false;
			}
			bool const is_number {
				code.value >= 48 && code.value <= 57 };

			bool const is_unit_prefix {
				code.value == 71 || code.value == 75 || code.value == 77 };

			return is_number || is_unit_prefix;
		}
};


class File_vault::Input_number_of_blocks : public Input_single_line
{
	public:

		void print(Output &out) const
		{
			_print_characters(out);
		}

		unsigned long to_unsigned_long() const
		{
			String<32> const str { *this };
			unsigned long result { 0 };
			ascii_to(str.string(), result);
			return result;
		}

		bool is_nr_greater_than_zero() const
		{
			return (size_t)to_unsigned_long() > 0;
		}

		bool appendable_character(Codepoint code)
		{
			if (!code.valid()) {
				return false;
			}
			bool const is_number {
				code.value >= 48 && code.value <= 57 };

			return is_number;
		}
};

#endif /* _INPUT_H_ */
