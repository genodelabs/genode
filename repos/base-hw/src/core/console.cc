/*
 * \brief  Genode-console backend
 * \author Martin Stein
 * \date   2011-10-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/console.h>
#include <base/printf.h>

/* core includes */
#include <serial.h>

/* base includes */
#include <unmanaged_singleton.h>

namespace Genode
{
	/**
	 * Platform specific Genode console
	 */
	class Platform_console : public Console, public Serial
	{
		enum { BAUD_RATE = 115200 };

		protected:

			/**
			 * Print a char to the console
			 */
			void _out_char(char c)
			{
				enum {
					ASCII_LINE_FEED = 10,
					ASCII_CARRIAGE_RETURN = 13,
				};

				/* auto complete new line commands */
				if (c == ASCII_LINE_FEED)
					Serial::put_char(ASCII_CARRIAGE_RETURN);

				/* print char */
				Serial::put_char(c);
			}

		public:

			/**
			 * Constructor
			 */
			Platform_console() : Serial(BAUD_RATE) { }
	};
}

using namespace Genode;


/**
 * Static object to print log output
 */
static Platform_console * platform_console()
{
	return unmanaged_singleton<Platform_console>();
}


/****************************
 ** Genode print functions **
 ****************************/

void Genode::printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);
	platform_console()->vprintf(format, list);
	va_end(list);
}


void Genode::vprintf(const char *format, va_list list)
{
	platform_console()->vprintf(format, list);
}

