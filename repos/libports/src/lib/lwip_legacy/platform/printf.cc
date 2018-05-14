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
#include <base/printf.h>
#include <base/console.h>
#include <base/lock.h>
#include <base/env.h>

extern "C" {

/* LwIP includes */
#include <arch/cc.h>

	/* Simply map to Genode's printf implementation */
	void lwip_printf(const char *format, ...)
	{
		va_list list;
		va_start(list, format);

		Genode::vprintf(format, list);

		va_end(list);
	}

}
