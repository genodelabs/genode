/*
 * \brief  Console backend for Microblaze
 * \author Martin Stein
 * \date   2011-02-22
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/console.h>
#include <base/printf.h>
#include <xilinx/xps_uartl.h>

namespace Genode {

	class Microblaze_console : public Console
	{
		private: 

			Xilinx::Xps_uartl _uart;

		protected:

			virtual void _out_char(char c)
			{
				_uart.send(c);
			}

		public:
 
			Microblaze_console() : _uart(0x84000000) {}
	};
}


using namespace Genode;


static Microblaze_console &microblaze_console()
{
	static Microblaze_console static_microblaze_console;
	return static_microblaze_console;
}


void Genode::printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	microblaze_console().vprintf(format, list);

	va_end(list);
}


void Genode::vprintf(const char *format, va_list list)
{
	microblaze_console().vprintf(format, list);
}
