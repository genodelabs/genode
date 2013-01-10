/*
 * \brief  Genode C API print functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-19
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/exception.h>

extern "C" {
#include <genode/printf.h>


	void genode_printf(const char *format, ...)
	{
		va_list list;
		va_start(list, format);
		Genode::vprintf(format, list);
	}

} // extern "C"
