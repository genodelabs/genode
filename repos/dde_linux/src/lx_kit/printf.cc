/*
 * \brief  Linux kit printf backend
 * \author Sebastian Sumpf
 * \date   2016-04-20
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* local includes */
#include <lx_emul.h>


namespace Lx {
	class Console;
	class Format_command;
}


/**
 * Format string command representation
 */
class Lx::Format_command
{
	public:

		enum Type   { INT, UINT, STRING, CHAR, PTR, PERCENT, VA_FORMAT, MAC,
		              IPV4, INVALID };
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

				case 'V': type = VA_FORMAT; break;
				case 'M': type = MAC; base = 16; padding = 2; break;

				case 'I':

					if (format[consumed + 1] != '4') break;
					consumed++;
					type = IPV4; base = 10;
					break;

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


class Lx::Console
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

					case Format_command::VA_FORMAT: /* %pV */
					{
						va_list va;
						va_format *vf = va_arg(list, va_format *);
						va_copy(va,  *vf->va);
						vprintf(vf->fmt, va);
						va_end(va);
					}
						break;

					case Format_command::MAC: /* %pM */
					{
						unsigned char const *mac = va_arg(list, unsigned char const *);
						for (int i = 0; i < 6; i++) {
							if (i) _out_char(':');
							_out_unsigned<unsigned char>(mac[i], cmd.base, cmd.padding);
						}
						break;
					}

					case Format_command::IPV4: /* %pI4 */
					{
						unsigned char const *ip = va_arg(list, unsigned char const *);
						for (int i = 0; i < 4; i++) {
							if (i) _out_char('.');
							_out_unsigned<unsigned char>(ip[i], cmd.base, cmd.padding);
						}
					}
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


void lx_printf(char const *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	Lx::Console::c().vprintf(fmt, va);
	va_end(va);
}


void lx_vprintf(char const *fmt, va_list va) {
	Lx::Console::c().vprintf(fmt, va); }
