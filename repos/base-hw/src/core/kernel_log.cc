/*
 * \brief  Access to the core's log facility
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>

#include <serial.h>
#include <kernel/log.h>


void Kernel::log(char const c)
{
	using Genode::Serial;

	enum {
		ASCII_LINE_FEED = 10,
		ASCII_CARRIAGE_RETURN = 13,
		BAUD_RATE = 115200
	};

	Serial & serial = *unmanaged_singleton<Serial>(BAUD_RATE);
	if (c == ASCII_LINE_FEED) serial.put_char(ASCII_CARRIAGE_RETURN);
	serial.put_char(c);
}
