/*
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \author Sebstian Sumpf
 * \date   2014-11-09
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/sleep.h>
#include <base/snprintf.h>
#include <util/string.h>

/* local includes */
#include <bsd_emul.h>

/* compiler includes */
#include <stdarg.h>


/*******************
 ** machine/mutex **
 *******************/

void mtx_enter(struct mutex *mtx) {
	mtx->mtx_owner = curcpu(); }


void mtx_leave(struct mutex *mtx) {
	mtx->mtx_owner = nullptr; }


/*****************
 ** sys/systm.h **
 *****************/

namespace Bsd {
	class Console;
	class Format_command;
}


/**
 * Format string command representation
 */
class Bsd::Format_command
{
	public:

		enum Type   { INT, UINT, STRING, CHAR, PTR, PERCENT,
		              INVALID };
		enum Length { DEFAULT, LONG, SIZE_T, LONG_LONG };

	private:

		/**
		 * Read decimal value from string
		 */
		int decode_decimal(const char *str, int *consumed)
		{
			int res = 0;
			while (1) {
				char c = str[*consumed];

				if (!c || c < '0' || c > '0' + 9)
					return res;

				res = (res * 10) + c - '0';
				(*consumed)++;
			}
		}

	public:

		Type   type      = INVALID;  /* format argument type               */
		Length length    = DEFAULT;  /* format argument length             */
		int    padding   = 0;        /* min number of characters to print  */
		int    base      = 10;       /* base of numeric arguments          */
		bool   zeropad   = false;    /* pad with zero instead of space     */
		bool   uppercase = false;    /* use upper case for hex numbers     */
		bool   prefix    = false;    /* prefix with 0x                     */
		int    consumed  = 0;        /* nb of consumed format string chars */

		/**
		 * Constructor
		 *
		 * \param format  begin of command in format string
		 */
		explicit Format_command(const char *format)
		{
			/* check for command begin and eat the character */
			if (format[consumed] != '%') return;
			if (!format[++consumed]) return;

			/* check for %$x syntax */
			prefix = (format[consumed] == '#') || (format[consumed] == '.');
			if (prefix && !format[++consumed]) return;

			/* heading zero indicates zero-padding */
			zeropad = (format[consumed] == '0');

			/* read decimal padding value */
			padding = decode_decimal(format, &consumed);
			if (!format[consumed]) return;

			/* decode length */
			switch (format[consumed]) {

				case 'l':
					{
						/* long long ints are marked by a subsequenting 'l' character */
						bool is_long_long = (format[consumed + 1] == 'l');

						length    = is_long_long ? LONG_LONG : LONG;
						consumed += is_long_long ? 2         : 1;
						break;
					}

				case 'z':
				case 'Z':

					length = SIZE_T;
					consumed++;
					break;

				case 'p':

					length = LONG;
					break;

				default: break;
			}

			if (!format[consumed]) return;

			/* decode type */
			switch (format[consumed]) {

				case 'd':
				case 'i': type =  INT;    base = 10; break;
				case 'o': type = UINT;    base =  8; break;
				case 'u': type = UINT;    base = 10; break;
				case 'x': type = UINT;    base = 16; break;
				case 'X': type = UINT;    base = 16; uppercase = 1; break;
				case 'p': type = PTR;     base = 16; break;
				case 'c': type = CHAR;    break;
				case 's': type = STRING;  break;
				case '%': type = PERCENT; break;

				case  0: return;
				default: break;
			}

			/* eat type character */
			consumed++;

			if (type != PTR || !format[consumed])
				return;

			switch (format[consumed]) {
				default: return;
			}

			consumed++;
		}

		int numeric()
		{
			return (type == INT || type == UINT || type == PTR);
		}
};


/**
 * Convert digit to ASCII value
 */
static char ascii(int digit, int uppercase = 0)
{
	if (digit > 9)
		return digit + (uppercase ? 'A' : 'a') - 10;

	return digit + '0';
}


class Bsd::Console
{
	private:

		enum { BUF_SIZE = 216 };

		char     _buf[BUF_SIZE + 1];
		unsigned _idx  = 0;

		void _flush()
		{
			if (!_idx)
				return;

			_buf[_idx] = 0;
			Genode::log(Genode::Cstring(_buf));
			_idx = 0;
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
				buf[i++] = ascii(0);

			/* fill buffer starting with the least significant digits */
			else
				for (; value > 0; value /= base)
					buf[i++] = ascii(value % base);

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
				buf[i++] = ascii(0);
				pad--;
			}

			/* fill buffer starting with the least significant digits */
			for (; value > 0; value /= base, pad--)
				buf[i++] = ascii(value % base);

			/* add padding zeros */
			for (; pad-- > 0; )
				_out_char(ascii(0));

			/* output buffer in reverse order */
			for (; i--; )
				_out_char(buf[i]);
		}

	protected:

		void _out_char(char c)
		{
			if (c == '\n' || _idx == BUF_SIZE || c == 0)
				_flush();
			else
				_buf[_idx++] = c;
		}

		void _out_string(const char *str)
		{
			if (str)
				while (*str) _out_char(*str++);
			else
				_flush();
		}

	public:

		static Console &c()
		{
			static Console _inst;
			return _inst;
		}

		void vprintf(const char *format, va_list list)
		{
			while (*format) {

				/* eat and output plain characters */
				if (*format != '%') {
					_out_char(*format++);
					continue;
				}

				/* parse format argument descriptor */
				Format_command cmd(format);

				/* read numeric argument from va_list */
				long long numeric_arg = 0;
				if (cmd.numeric()) {
					switch (cmd.length) {

						case Format_command::LONG_LONG:

							numeric_arg = va_arg(list, long long);
							break;

						case Format_command::LONG:

							numeric_arg = (cmd.type == Format_command::UINT) ?
								(long long)va_arg(list, unsigned long) : va_arg(list, long);
							break;

						case Format_command::SIZE_T:

							numeric_arg = va_arg(list, size_t);
							break;

						case Format_command::DEFAULT:

							numeric_arg = (cmd.type == Format_command::UINT) ?
								(long long)va_arg(list, unsigned int) : va_arg(list, int);
							break;
					}
				}

				/* call type-specific output routines */
				switch (cmd.type) {

					case Format_command::INT:

						if (cmd.length == Format_command::LONG_LONG)
							_out_signed<long long>(numeric_arg, cmd.base);
						else
							_out_signed<long>(numeric_arg, cmd.base);
						break;

					case Format_command::UINT:

						if (cmd.prefix && cmd.base == 16)
							_out_string("0x");

						if (cmd.length == Format_command::LONG_LONG) {

							_out_unsigned<unsigned long long>(numeric_arg, cmd.base, cmd.padding);
							break;
						}

						/* fall through */

					case Format_command::PTR:

						_out_unsigned<unsigned long>(numeric_arg, cmd.base, cmd.padding);
						break;

					case Format_command::CHAR:

						_out_char(va_arg(list, int));
						break;

					case Format_command::STRING:

						_out_string(va_arg(list, const char *));
						break;

					case Format_command::PERCENT:

						_out_char('%');
						break;

					case Format_command::INVALID:

						_out_string("<warning: unsupported format string argument>");
						/* consume the argument of the unsupported command */
						va_arg(list, long);
						break;
				}

				/* proceed with format string after command */
				format += cmd.consumed;
			}
		}
};


extern "C" void panic(char const *format, ...)
{
	va_list list;

	va_start(list, format);
	Bsd::Console::c().vprintf(format, list);
	va_end(list);

	Genode::sleep_forever();
}


extern "C" int printf(const char *format, ...)
{
	va_list list;

	va_start(list, format);
	Bsd::Console::c().vprintf(format, list);
	va_end(list);

	/* hopefully this gets never checked... */
	return 0;
}


extern "C" int snprintf(char *str, size_t size, const char *format, ...)
{
	va_list list;

	va_start(list, format);
	Genode::String_console sc(str, size);
	sc.vprintf(format, list);
	va_end(list);

	return sc.len();
}


extern "C" int strcmp(const char *s1, const char *s2)
{
	return Genode::strcmp(s1, s2);
}


/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

extern "C" size_t strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';              /* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);    /* count does not include NUL */
}
