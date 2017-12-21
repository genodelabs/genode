/*
 * \brief  Interface for textual output
 * \author Norman Feske
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__OUTPUT_H_
#define _INCLUDE__BASE__OUTPUT_H_

#include <base/stdint.h>
#include <util/interface.h>

namespace Genode { struct Output; }


struct Genode::Output : Interface
{
	/**
	 * Output single character
	 */
	virtual void out_char(char) = 0;

	/**
	 * Output string
	 *
	 * \param n  maximum number of characters to output
	 *
	 * The output stops on the first occurrence of a null character in the
	 * string or after 'n' characters.
	 *
	 * The default implementation uses 'out_char'. This method may be
	 * overridden by the backend for improving efficiency.
	 */
	virtual void out_string(char const *str, size_t n = ~0UL);

	/**
	 * Helper for the sequential output of a variable list of arguments
	 */
	template <typename HEAD, typename... TAIL>
	static void out_args(Output &output, HEAD && head, TAIL &&... tail)
	{
		print(output, head);
		out_args(output, tail...);
	}

	template <typename LAST>
	static void out_args(Output &output, LAST && last) { print(output, last); }
};


namespace Genode {

	/**
	 * Print null-terminated string
	 */
	void print(Output &output, char const *);

	/**
	 * Disallow printing non-const character buffers
	 *
	 * For 'char *' types, it is unclear whether the argument should be printed
	 * as a pointer or a string. The call must resolve this ambiguity by either
	 * casting the argument to 'void *' or wrapping it in a 'Cstring' object.
	 */
	void print(Output &, char *) = delete;

	/**
	 * Print pointer value
	 */
	void print(Output &output, void const *);

	/**
	 * Print arbitrary pointer types
	 *
	 * This function template takes precedence over the one that takes a
	 * constant object reference as argument.
	 */
	template <typename T>
	static inline void print(Output &output, T *ptr)
	{
		print(output, (void const *)ptr);
	}

	/**
	 * Print unsigned long value
	 */
	void print(Output &output, unsigned long);

	/**
	 * Print unsigned long long value
	 */
	void print(Output &output, unsigned long long);

	/*
	 * Overloads for unsigned integer types
	 */
	static inline void print(Output &o, unsigned char  v) { print(o, (unsigned long)v); }
	static inline void print(Output &o, unsigned short v) { print(o, (unsigned long)v); }
	static inline void print(Output &o, unsigned int   v) { print(o, (unsigned long)v); }

	/**
	 * Print signed long value
	 */
	void print(Output &output, long);

	/**
	 * Print signed long long value
	 */
	void print(Output &output, long long);

	/*
	 * Overloads for signed integer types
	 */
	static inline void print(Output &o, char  v) { print(o, (long)v); }
	static inline void print(Output &o, short v) { print(o, (long)v); }
	static inline void print(Output &o, int   v) { print(o, (long)v); }

	/**
	 * Print bool value
	 */
	static inline void print(Output &output, bool value)
	{
		print(output, (int)value);
	}

	/**
	 * Print single-precision float
	 */
	void print(Output &output, float);

	/**
	 * Print double-precision float
	 */
	void print(Output &output, double);

	/**
	 * Helper for the hexadecimal output of integer values
	 *
	 * To output an integer value as hexadecimal number, the value can be
	 * wrapped into an 'Hex' object, thereby selecting the corresponding
	 * overloaded 'print' function below.
	 */
	class Hex
	{
		public:

			enum Prefix { PREFIX, OMIT_PREFIX };
			enum Pad    { PAD,    NO_PAD };

		private:

			unsigned long long const _value;
			size_t             const _digits;
			Prefix             const _prefix;
			Pad                const _pad;

		public:

			/**
			 * Constructor
			 *
			 * \param prefix  by default, the value is prepended with the prefix
			 *                '0x'. The prefix can be suppressed by specifying
			 *                'OMIT_PREFIX' as argument.
			 * \param pad     by default, leading zeros are stripped from the
			 *                output. If set to 'PAD', the leading zeros will be
			 *                printed.
			 */
			template <typename T>
			explicit Hex(T value, Prefix prefix = PREFIX, Pad pad = NO_PAD)
			: _value(value), _digits(2*sizeof(T)), _prefix(prefix), _pad(pad) { }

			void print(Output &output) const;
	};

	/**
	 * Print range as hexadecimal format
	 *
	 * This helper is intended for the output for memory-address ranges. For
	 * brevity, it omits the '0x' prefix from the numbers. The numbers are
	 * padded with leading zeros to foster the horizontal alignment of
	 * consecutive outputs (like a table of address ranges).
	 */
	template <typename T>
	struct Hex_range
	{
		T const base;
		size_t const len;

		Hex_range(T base, size_t len) : base(base), len(len) { }

		void print(Output &out) const;
	};

	/**
	 * Helper for the output of an individual character
	 *
	 * When printing a 'char' value, it appears as an integral number. By
	 * wrapping the value in a 'Char' object, it appears as character instead.
	 */
	struct Char
	{
		char const c;

		explicit Char(char c) : c(c) { }

		void print(Output &output) const { output.out_char(c); }
	};

	/**
	 * Print information about object 'obj'
	 *
	 * The object type must provide a const 'print(Output &)' method that
	 * produces the textual representation of the object.
	 *
	 * In contrast to overloads of the 'Genode::print' function, the 'T::print'
	 * method is able to access object-internal state, which can thereby be
	 * incorporated into the textual output.
	 */
	template <typename T>
	static inline void print(Output &output, T const &obj)
	{
		obj.print(output);
	}

	/**
	 * Print a variable number of arguments
	 */
	template <typename HEAD, typename... TAIL>
	static inline void print(Output &output, HEAD const &head, TAIL &&... tail)
	{
		Output::out_args(output, head, tail...);
	}
}


template <typename T>
void Genode::Hex_range<T>::print(Output &out) const
{
	using Genode::print;

	Hex const from(base, Hex::OMIT_PREFIX, Hex::PAD);

	T const end = base + len;

	/* if end at integer limit, use ']' as closing delimiter */
	if (base && end == 0) {
		Hex const inclusive_to((T)(end - 1), Hex::OMIT_PREFIX, Hex::PAD);
		print(out, "[", from, ",", inclusive_to, "]");
		return;
	}

	/* use exclusive upper limit for ordinary ranges  */
	print(out, "[", from, ",", Hex(end, Hex::OMIT_PREFIX, Hex::PAD), ")");

	/* output warning on integer-overflowing upper limit or empty range */
	if (base && end < base) print(out, " (overflow!)");
	if (len == 0)           print(out, " (empty!)");
}

#endif /* _INCLUDE__BASE__OUTPUT_H_ */
