/*
 * \brief  L4Re functions needed by L4Linux.
 * \author Stefan Kalkowski
 * \date   2011-04-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

namespace Fiasco {
#include <l4/log/log.h>
#include <l4/sys/kdebug.h>
}

using namespace Fiasco;

extern "C" {

	void LOG_printf(const char *format, ...)
	{
		va_list list;
		va_start(list, format);
		Genode::vprintf(format, list);
	}


	void LOG_vprintf(const char *format, va_list list)
	{
		Genode::vprintf(format, list);
	}


	void LOG_flush(void) {}

}
