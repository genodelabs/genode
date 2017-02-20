/*
 * \brief  C implementation of 'Genode::printf()'
 * \author Christian Prochaska
 * \date   2013-07-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/printf.h>

extern "C" void genode_printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);
	Genode::vprintf(format, list);
	va_end(list);
}
