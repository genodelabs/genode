/*
 * \brief  Output of format strings
 * \author Norman Feske
 * \date   2006-04-07
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* format library interface */
#include <format/console.h>

/* Genode includes */
#include <base/stdint.h>
#include <output.h>       /* base/internal/output.h */

using namespace Format;
using namespace Genode;


/**
 * Format string command representation
 */
class Format_command
{
	public:

		enum Type   { INT, UINT, STRING, CHAR, PTR, PERCENT, INVALID };
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
		int    precision = 0;        /* max number of characters to print  */
		int    base      = 10;       /* base of numeric arguments          */
		bool   lalign    = false;    /* align left                         */
		bool   zeropad   = false;    /* pad with zero instead of space     */
		bool   uppercase = false;    /* use upper case for hex numbers     */
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

			/* read left alignment */
			if (format[consumed] == '-') {
				lalign = true;
				if (!format[++consumed]) return;
			}

			/* heading zero indicates zero-padding */
			zeropad = (format[consumed] == '0');

			/* read decimal padding value */
			padding = decode_decimal(format, &consumed);
			if (!format[consumed]) return;

			/* read precision value */
			if (format[consumed] == '.')
				precision = decode_decimal(format, &(++consumed));
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
		}

		int numeric()
		{
			return (type == INT || type == UINT || type == PTR);
		}
};


void Console::_out_string(const char *str)
{
	if (!str)
		_out_string("<NULL>");
	else
		while (*str) _out_char(*str++);
}


void Console::printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);
	vprintf(format, list);
	va_end(list);
}


void Console::vprintf(const char *format, va_list list)
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
					out_signed<long long>(numeric_arg, cmd.base,
					                      [&] (char c) { _out_char(c); });
				else
					out_signed<long>((long)numeric_arg, cmd.base,
					                 [&] (char c) { _out_char(c); });
				break;

			case Format_command::UINT:

				if (cmd.length == Format_command::LONG_LONG) {
					out_unsigned<unsigned long long>(numeric_arg, cmd.base, cmd.padding,
					                                 [&] (char c) { _out_char(c); });
					break;
				}

				/* fall through */

			case Format_command::PTR:

				out_unsigned<unsigned long>((long)numeric_arg, cmd.base, cmd.padding,
				                            [&] (char c) { _out_char(c); });
				break;

			case Format_command::CHAR:

				_out_char((char)va_arg(list, int));
				break;

			case Format_command::STRING:

				if (cmd.precision) {
					int p = 0;
					char const *a = va_arg(list, const char *);
					for (; a && *a && p < cmd.precision; ++a, ++p)
						_out_char(*a);
					for (; p < cmd.padding; ++p)
						_out_char(' ');
				} else {
					_out_string(va_arg(list, const char *));
				}
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
