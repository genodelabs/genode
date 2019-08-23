/*
 * \brief  Genode utility support
 * \author Christian Helmuth <christian.helmuth@genode-labs.com>
 * \date   2019-07-25
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/console.h>

#include "util.h"


void Genode::Console::_out_string(char const *str)
{
	if (!str)
		_out_string("<NULL>");
	else
		while (*str) _out_char(*str++);
}


void Genode::Console::printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);
	vprintf(format, list);
	va_end(list);
}


void Genode::Console::vprintf(const char *format, va_list list)
{
	Formatted str(format, list);

	_out_string(str.string());
}
