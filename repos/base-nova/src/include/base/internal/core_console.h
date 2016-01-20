/*
 * \brief  Console backend for NOVA
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2009-12-28
 */

/*
 * Copyright (C) 2009-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#pragma once

/* Genode includes */
#include <base/console.h>
#include <bios_data_area.h>
#include <drivers/uart_base.h>

namespace Genode { class Core_console; }

class Genode::Core_console : public X86_uart_base, public Console
{
	private:

		enum { CLOCK = 0, BAUDRATE = 115200 };

		void _out_char(char c)
		{
			if (c == '\n')
				put_char('\r');

			put_char(c);
		}

	public:

		Core_console()
		:
			X86_uart_base(Bios_data_area::singleton()->serial_port(),
			              CLOCK, BAUDRATE)
		{ }
};
