/*
 * \brief  Print to kernel log output
 * \author Martin stein
 * \date   2012-04-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__KERNEL__LOG_H_
#define _INCLUDE__KERNEL__LOG_H_

/* Genode includes */
#include <kernel/syscalls.h>

namespace Genode
{
	/**
	 * Prints incoming streams to the kernel log output
	 */
	class Kernel_log
	{
		/**
		 * Print an unsigned 4 bit integer as hexadecimal value
		 */
		void _print_4bit_hex(unsigned char x) const
		{
			/* decode to ASCII char */
			x &= 0x0f;
			if (x > 9) x += 39;
			x += 48;

			/* print ASCII char */
			Kernel::print_char(x);
		}

		public:

			/**
			 * Print zero-terminated string
			 */
			Kernel_log & operator << (char const * s)
			{
				while (*s) Kernel::print_char(*s++);
				return *this;
			}

			/**
			 * Print an unsigned integer as hexadecimal value
			 */
			Kernel_log & operator << (unsigned int const & x)
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
				return *this;
			}
	};

	/**
	 * Give static 'Kernel_log' reference as target for common log output
	 */
	inline Kernel_log & kernel_log()
	{
		static Kernel_log _log;
		return _log;
	}
}

#endif /* _INCLUDE__KERNEL__LOG_H_  */

