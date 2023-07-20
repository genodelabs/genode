/*
 * \brief  Utilities for formatted text output
 * \author Norman Feske
 * \date   2022-03-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__UTIL__FORMATTED_OUTPUT_H_
#define _INCLUDE__OS__UTIL__FORMATTED_OUTPUT_H_

#include <base/output.h>
#include <util/string.h>

namespace Genode {

	template <typename> struct Truncated;
	template <typename> struct Repeated;
	template <typename> struct Left_aligned;
	template <typename> struct Right_aligned;

	class Hex_dump;

	/**
	 * Return the number of characters needed when rendering 'args' as text
	 */
	template <typename... ARGS>
	static unsigned printed_length(ARGS &&... args)
	{
		struct _Output : Output
		{
			unsigned count = 0;
			void out_char(char) override { count++; }
		} _output;

		print(_output, args...);
		return _output.count;
	}
}


template <typename T>
struct Genode::Truncated
{
	unsigned const  _limit;
	T        const &_arg;

	Truncated(unsigned limit, T const &arg) : _limit(limit), _arg(arg) { }

	void print(Output &out) const
	{
		struct _Output : Output
		{
			Output        &out;
			unsigned const limit;
			unsigned       count = 0;

			void out_char(char c) override
			{
				if (count++ < limit)
					out.out_char(c);
			}

			_Output(Output &out, unsigned limit) : out(out), limit(limit) { }

		} _output(out, _limit);

		Genode::print(_output, _arg);
	}
};


template <typename T>
struct Genode::Repeated
{
	unsigned const  _n;
	T        const &_arg;

	Repeated(unsigned n, T const &arg) : _n(n), _arg(arg) { }

	void print(Output &out) const
	{
		for (unsigned i = 0; i < _n; i++)
			Genode::print(out, _arg);
	}
};


template <typename T>
struct Genode::Left_aligned
{
	unsigned const  _n;
	T        const &_arg;

	Left_aligned(unsigned n, T const &arg) : _n(n), _arg(arg) { }

	void print(Output &out) const
	{
		unsigned const len = min(printed_length(_arg), _n);
		Genode::print(out, Truncated(len, _arg), Repeated(_n - len, Char(' ')));
	}
};


template <typename T>
struct Genode::Right_aligned
{
	unsigned const  _n;
	T        const &_arg;

	Right_aligned(unsigned n, T const &arg) : _n(n), _arg(arg) { }

	void print(Output &out) const
	{
		unsigned const len = min(printed_length(_arg), _n);
		Genode::print(out, Repeated(_n - len, Char(' ')), Truncated(len, _arg));
	}
};


class Genode::Hex_dump
{
	private:

		static constexpr size_t _max_bytes_per_line { 16 };
		uint8_t const * const _base;
		size_t const _size;
		size_t const _num_lines { (_size + _max_bytes_per_line - 1) / _max_bytes_per_line };

		static bool _printable_ascii(char c)
		{
			return c > 31 && c < 127;
		}

		static void _print_line_as_hex_words(Output &out, uint8_t const *base, size_t size)
		{
			using Genode::print;
			static constexpr size_t max_bytes_per_word { 2 };
			for (size_t idx { 0 }; idx < _max_bytes_per_line; idx++) {

				if (idx && idx % max_bytes_per_word == 0)
					print(out, " ");

				if (idx < size)
					print(out, Hex(base[idx], Hex::OMIT_PREFIX, Hex::PAD));
				else
					print(out, "  ");
			}
		}

		static void _print_line_as_ascii(Output &out, uint8_t const *base, size_t size)
		{
			using Genode::print;
			for (size_t idx { 0 }; idx < size; idx++) {

				char const *char_ptr { (char *)&base[idx] };
				if (_printable_ascii(*char_ptr))
					print(out, Cstring { char_ptr, 1 });
				else
					print(out, ".");
			}
		}

		static void _print_line_offset(Output &out, size_t line_offset)
		{
			Genode::print(out, Hex((uint32_t)line_offset, Hex::OMIT_PREFIX, Hex::PAD));
		}

	public:

		Hex_dump(auto const &range)
		: _base { (uint8_t *)range.start }, _size { range.num_bytes } { }

		void print(Output &out) const
		{
			using Genode::print;
			uint8_t const *prev_line_ptr { nullptr };
			bool prev_line_was_duplicate { false };
			for (size_t line_idx { 0 }; line_idx < _num_lines; line_idx++) {

				size_t const line_offset { line_idx * _max_bytes_per_line };
				size_t const line_size { min((size_t)_max_bytes_per_line, _size - line_offset) };
				uint8_t const *line_ptr { &_base[line_offset] };
				bool const last_line { line_idx == _num_lines - 1 };

				bool line_is_duplicate { false };
				if (prev_line_ptr)
					line_is_duplicate = !memcmp(prev_line_ptr, line_ptr, line_size);

				if (!line_is_duplicate || last_line) {

					_print_line_offset(out, line_offset);
					print(out, ": ");
					_print_line_as_hex_words(out, line_ptr, line_size);
					print(out, "  ");
					_print_line_as_ascii(out, line_ptr, line_size);
					print(out, last_line ? "" : "\n");
				}
				if (line_is_duplicate && !prev_line_was_duplicate && !last_line)
					print(out, "*\n");


				prev_line_ptr = line_ptr;
				prev_line_was_duplicate = line_is_duplicate;
			}
		}
};

#endif /* _INCLUDE__OS__UTIL__FORMATTED_OUTPUT_H_ */
