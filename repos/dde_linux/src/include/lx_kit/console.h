/*
 * \brief  Lx_kit format string backend
 * \author Stefan Kalkowski
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2021-03-17
 *
 * Greatly inspired by the former DDE Linux Lx_kit implementation.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__CONSOLE_H_
#define _LX_KIT__CONSOLE_H_

#include <stdarg.h>

namespace Lx_kit {
	class Console;

	using namespace Genode;
}


class Lx_kit::Console
{
	private:

		enum { BUF_SIZE = 216 };

		char     _buf[BUF_SIZE + 1];
		unsigned _idx  = 0;

		/**
		 * Convert digit to ASCII value
		 */
		static inline char _ascii(int digit, int uppercase = 0)
		{
			if (digit > 9)
				return (char)(digit + (uppercase ? 'A' : 'a') - 10);

			return (char)(digit + '0');
		}

		/**
		 * Output signed value with the specified base
		 */
		template <typename T>
		void _out_signed(T value, unsigned base)
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
				buf[i++] = _ascii(0);

			/* fill buffer starting with the least significant digits */
			else
				for (; value > 0; value /= base)
					buf[i++] = _ascii((value % base) & 0xff);

			/* add sign to buffer for negative values */
			if (neg)
				_out_char('-');

			/* output buffer in reverse order */
			for (; i--; )
				_out_char(buf[i]);
		}


		/**
		 * Output unsigned value with the specified base and padding
		 */
		template <typename T>
		void _out_unsigned(T value, unsigned base, int pad)
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
				buf[i++] = _ascii(0);
				pad--;
			}

			/* fill buffer starting with the least significant digits */
			for (; value > 0; value /= (T)base, pad--)
				buf[i++] = _ascii((value % base) & 0xff);

			/* add padding zeros */
			for (; pad-- > 0; )
				_out_char(_ascii(0));

			/* output buffer in reverse order */
			for (; i--; )
				_out_char(buf[i]);
		}

		void _out_char(char c);
		void _out_string(const char *str);
		void _flush();

	public:

		void vprintf(const char *format, va_list list);
		void print_string(const char *str);
};

#endif /* _LX_KIT__CONSOLE_H_ */
