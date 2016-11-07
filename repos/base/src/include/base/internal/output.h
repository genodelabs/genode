/*
 * \brief  Internal utilities used for implementing the 'Output' funtions
 * \author Norman Feske
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__OUTPUT_H_
#define _INCLUDE__BASE__INTERNAL__OUTPUT_H_


#include <base/output.h>

using namespace Genode;


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
	/* set flag if value is negative */
	int neg = value < 0 ? 1 : 0;

	/* get absolute value */
	value = value < 0 ? -value : value;

	uint64_t integer = (uint64_t)value;

	if (neg)
		out_char('-');

	out_unsigned(integer, base, 0, out_char);
	out_char('.');

	if (length) {
		do {
			value -= integer;
			value = value*base;

			integer = (int64_t)value;
			out_char(ascii(integer));

			length--;
		} while (length && (value > 0.0));
	}
}

namespace Genode { template <size_t, typename> class Buffered_output; }


/**
 * Implementation of the output interface that buffers characters
 *
 * \param BUF_SIZE  maximum number of characters to buffer before writing
 * \param WRITE_FN  functor called to writing the buffered characters to a
 *                  backend.
 *
 * The 'WRITE_FN' functor is called with a null-terminated 'char const *'
 * as argument.
 */
template <size_t BUF_SIZE, typename BACKEND_WRITE_FN>
class Genode::Buffered_output : public Output
{
	private:

		BACKEND_WRITE_FN _write_fn;
		char             _buf[BUF_SIZE];
		unsigned         _num_chars = 0;

		void _flush()
		{
			/* null-terminate string */
			_buf[_num_chars] = 0;
			_write_fn(_buf);

			/* restart with empty buffer */
			_num_chars = 0;
		}

	public:

		Buffered_output(BACKEND_WRITE_FN const &write_fn) : _write_fn(write_fn) { }

		void out_char(char c) override
		{
			/* ensure enough buffer space for complete escape sequence */
			if ((c == 27) && (_num_chars + 8 > BUF_SIZE)) _flush();

			_buf[_num_chars++] = c;

			/* flush immediately on line break */
			if (c == '\n' || _num_chars >= sizeof(_buf) - 1)
				_flush();
		}
};

#endif /* _INCLUDE__BASE__INTERNAL__OUTPUT_H_ */
