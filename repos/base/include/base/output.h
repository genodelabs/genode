/*
 * \brief  Interface for textual output
 * \author Norman Feske
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__OUTPUT_H_
#define _INCLUDE__BASE__OUTPUT_H_

#include <util/string.h>
#include <base/stdint.h>

namespace Genode { struct Output; }


struct Genode::Output
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
};


namespace Genode {

	/**
	 * Print null-terminated string
	 */
	void print(Output &output, char const *);

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
	 * Print unsigned integer value
	 */
	static inline void print(Output &output, unsigned value)
	{
		print(output, (unsigned long)value);
	}

	/**
	 * Print signed long value
	 */
	void print(Output &output, long);

	/**
	 * Print signed integer value
	 */
	static inline void print(Output &output, int value)
	{
		print(output, (long)value);
	}

	/**
	 * Print signed long long value
	 */
	void print(Output &output, long long);

	/**
	 * Helper for the hexadecimal output of integer values
	 *
	 * To output an integer value as hexadecimal number, the value can be
	 * wrapped into an 'Hex' object, thereby selecting the corresponding
	 * overloaded 'print' function below.
	 */
	struct Hex
	{
		enum Prefix { PREFIX, OMIT_PREFIX };
		enum Pad    { PAD,    NO_PAD };

		unsigned long const value;
		size_t        const digits;
		Prefix        const prefix;
		Pad           const pad;

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
		: value(value), digits(2*sizeof(T)), prefix(prefix), pad(pad) { }
	};

	/**
	 * Print hexadecimal number
	 */
	void print(Output &output, Hex const &);

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
}

#endif /* _INCLUDE__BASE__OUTPUT_H_ */
