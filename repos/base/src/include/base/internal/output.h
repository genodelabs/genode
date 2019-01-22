/*
 * \brief  Internal utilities used for implementing the 'Output' funtions
 * \author Norman Feske
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__OUTPUT_H_
#define _INCLUDE__BASE__INTERNAL__OUTPUT_H_

#include <base/output.h>


/**
 * Convert digit to ASCII value
 */
static inline char ascii(int digit, int uppercase = 0)
{
	if (digit > 9)
		return digit + (uppercase ? 'A' : 'a') - 10;

	return digit + '0';
}


/**
 * Output signed value with the specified base
 */
template <typename T, typename OUT_CHAR_FN>
static inline void out_signed(T value, unsigned base, OUT_CHAR_FN const &out_char)
{
	/**
	 * for base 8, the number of digits is the number of value bytes times 3
	 * at a max, because 0xff is 0o377 and accumulating this implies a
	 * strictly decreasing factor
	 */
	char buf[sizeof(value)*3];

	/* set flag if value is negative */
	int neg = value < 0 ? 1 : 0;

	/* get absolute value */
	value = value < 0 ? -value : value;

	int i = 0;

	/* handle zero as special case */
	if (value == 0)
		buf[i++] = ascii(0);

	/* fill buffer starting with the least significant digits */
	else
		for (; value > 0; value /= base)
			buf[i++] = ascii(value % base);

	/* add sign to buffer for negative values */
	if (neg)
		out_char('-');

	/* output buffer in reverse order */
	for (; i--; )
		out_char(buf[i]);
}


/**
 * Output unsigned value with the specified base and padding
 */
template <typename T, typename OUT_CHAR_FN>
static inline void out_unsigned(T value, unsigned base, int pad,
                                OUT_CHAR_FN const &out_char)
{
	/**
	 * for base 8, the number of digits is the number of value bytes times 3
	 * at a max, because 0xff is 0o377 and accumulating this implies a
	 * strictly decreasing factor
	 */
	char buf[sizeof(value)*3];

	int i = 0;

	/* handle zero as special case */
	if (value == 0) {
		buf[i++] = ascii(0);
		pad--;
	}

	/* fill buffer starting with the least significant digits */
	for (; value > 0; value /= base, pad--)
		buf[i++] = ascii(value % base);

	/* add padding zeros */
	for (; pad-- > 0; )
		out_char(ascii(0));

	/* output buffer in reverse order */
	for (; i--; )
		out_char(buf[i]);
}


/**
 * Output floating point value
 */
template <typename T, typename OUT_CHAR_FN>
static inline void out_float(T value, unsigned base, unsigned length, OUT_CHAR_FN const &out_char)
{
	using namespace Genode;

	/*
	 * If the compiler decides to move a value from the FPU to the stack and
	 * back, the value can slightly change because of different encodings. This
	 * can cause problems if the value is assumed to stay the same in a chain
	 * of calculations. For example, if casting 6.999 to int results in 6, this
	 * number 6 needs to be subtracted from 6.999 in the next step and not from
	 * 7 after an unexpected conversion, otherwise the next cast for a decimal
	 * place would result in 10 instead of 9.
	 * By storing the value in a volatile variable, the conversion step between
	 * FPU and stack happens in a more deterministic way, which gives more
	 * consistent results with this function.
	 */
	T volatile volatile_value = value;

	/* set flag if value is negative */
	int neg = volatile_value < 0 ? 1 : 0;

	/* get absolute value */
	volatile_value = volatile_value < 0 ? -volatile_value : volatile_value;

	uint64_t integer = (uint64_t)volatile_value;

	if (neg)
		out_char('-');

	out_unsigned(integer, base, 0, out_char);
	out_char('.');

	if (length) {
		do {
			volatile_value -= integer;
			volatile_value = volatile_value*base;

			integer = (int64_t)volatile_value;
			out_char(ascii(integer));

			length--;
		} while (length && (volatile_value > 0.0));
	}
}

#endif /* _INCLUDE__BASE__INTERNAL__OUTPUT_H_ */
