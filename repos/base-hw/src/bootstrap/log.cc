/*
 * \brief  Access to the log facility
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/buffered_output.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/output.h>
#include <base/internal/raw_write_string.h>
#include <base/internal/unmanaged_singleton.h>

#include <board.h>

using namespace Board;
using namespace Genode;

struct Buffer
{
	struct Write_fn
	{
		Serial & serial;

		Write_fn(Serial & serial) : serial(serial) {}

		void operator () (char const *s)
		{
			enum { LINE_FEED = 10, CARRIAGE_RETURN = 13 };

			for (unsigned i = 0; i < Genode::strlen(s); i++) {
				if (s[i] == LINE_FEED) serial.put_char(CARRIAGE_RETURN);
				serial.put_char(s[i]);
			}
		}
	};

	enum { BAUD_RATE = 115200 };

	Serial                         serial { UART_BASE, UART_CLOCK, BAUD_RATE };
	Write_fn                       function { serial   };
	Buffered_output<512, Write_fn> buffer   { function };
	Log                            log      { buffer   };
};


Genode::Log &Genode::Log::log() {
	return unmanaged_singleton<Buffer>()->log; }


void Genode::raw_write_string(char const *str) { log(str); }
