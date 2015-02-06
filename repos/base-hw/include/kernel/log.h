/*
 * \brief  Print to the standard output of the kernel
 * \author Martin stein
 * \date   2012-04-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__LOG_H_
#define _KERNEL__LOG_H_

/* Genode includes */
#include <kernel/interface.h>

namespace Kernel
{
	/**
	 * Prints incoming streams to the standard output of the kernel
	 */
	class Log
	{
		private:

			/**
			 * Print an unsigned 4 bit integer x as hexadecimal value
			 */
			void _print_4bit_hex(unsigned char x) const
			{
				/* decode to ASCII char */
				x &= 0x0f;
				if (x > 9) x += 39;
				x += 48;

				/* print ASCII char */
				print_char(x);
			}

		public:

			/**
			 * Print a zero-terminated string s
			 */
			Log & operator << (char const * s)
			{
				while (*s) print_char(*s++);
				if (*--s != '\n') { print_char(' '); }
				return *this;
			}

			/**
			 * Print an unsigned integer x as hexadecimal value
			 */
			Log & operator << (unsigned long const x)
			{
				enum {
					BYTE_WIDTH = 8,
					CW         = sizeof(unsigned char)*BYTE_WIDTH,
					IW         = sizeof(unsigned int)*BYTE_WIDTH
				};

				/*
				 * Walk from most significant to least significant bit and
				 * process 2 hex digits per step.
				 */
				bool leading = true;
				for (int i = IW - CW; i >= 0; i = i - CW)
				{
					/* fetch the 2 current digits */
					unsigned char c = (char)((x >> i) & 0xff);

					/* has the payload part of the value already begun? */
					if (leading)
					{
						/* ignore leading zeros ... */
						if (!c)
						{
							/* ... expect they are the only digits */
							if (i == 0) _print_4bit_hex(c);
							continue;
						}
						/* transit from leading to payload part */
						leading = false;
						if (c < 0x10) {
							_print_4bit_hex(c);
							continue;
						}
					}
					/* print both digits */
					_print_4bit_hex(c >> 4);
					_print_4bit_hex(c);
				}
				print_char(' ');
				return *this;
			}

			/**
			 * Print a pointer p as hexadecimal value
			 */
			Log & operator << (void * const p) { return *this << (unsigned long)p; }
	};

	/**
	 * Return singleton kernel-log
	 */
	inline Log & log()
	{
		static Log s;
		return s;
	}
}

#endif /* _KERNEL__LOG_H_  */

