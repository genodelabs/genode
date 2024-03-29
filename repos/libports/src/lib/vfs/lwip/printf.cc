/*
 * \brief  Print function for debugging functionality of lwIP.
 * \author Stefan Kalkowski
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/string.h>
#include <base/log.h>

/* format-string includes */
#include <format/snprintf.h>

extern "C" {

/* LwIP includes */
#include <arch/cc.h>

	/* Simply map to Genode's printf implementation */
	void lwip_printf(const char *format, ...)
	{
		va_list list;
		va_start(list, format);

		char buf[128] { };
		Format::String_console(buf, sizeof(buf)).vprintf(format, list);
		Genode::log(Genode::Cstring(buf));

		va_end(list);
	}

}
