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

#include <base/console.h>

/* Genode includes */
#include <drivers/uart/x86_uart_base.h>

namespace Genode { class Core_console; }

class Genode::Core_console : public X86_uart_base, public Console
{
	private:

		addr_t _port()
		{
			/**
			 * Read BDA (Bios Data Area) to obtain I/O ports of COM
			 * interfaces. The page must be mapped by the platform code !
			 */

			enum {
				MAP_ADDR_BDA         = 0x1000,

				BDA_SERIAL_BASE_COM1 = 0x400,
				BDA_EQUIPMENT_WORD   = 0x410,
				BDA_EQUIPMENT_SERIAL_COUNT_MASK  = 0x7,
				BDA_EQUIPMENT_SERIAL_COUNT_SHIFT = 9,
			};

			char * map_bda = reinterpret_cast<char *>(MAP_ADDR_BDA);
			uint16_t serial_count = *reinterpret_cast<uint16_t *>(map_bda + BDA_EQUIPMENT_WORD);
			serial_count >>= BDA_EQUIPMENT_SERIAL_COUNT_SHIFT;
			serial_count &= BDA_EQUIPMENT_SERIAL_COUNT_MASK;
			
			if (serial_count > 0)
				return *reinterpret_cast<uint16_t *>(map_bda + BDA_SERIAL_BASE_COM1);

			return 0;
		}

		enum { CLOCK = 0, BAUDRATE = 115200 };

		void _out_char(char c)
		{
			if (c == '\n')
				put_char('\r');

			put_char(c);
		}

	public:

		Core_console() : X86_uart_base(_port(), CLOCK, BAUDRATE) {} 
};
