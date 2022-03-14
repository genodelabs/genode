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

namespace Genode {

	template <typename> struct Truncated;
	template <typename> struct Repeated;
	template <typename> struct Left_aligned;
	template <typename> struct Right_aligned;

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

#endif /* _INCLUDE__OS__UTIL__FORMATTED_OUTPUT_H_ */
