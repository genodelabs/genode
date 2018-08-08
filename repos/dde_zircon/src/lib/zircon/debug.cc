/*
 * \brief  Zircon debug functions
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <util/string.h>

#include <zircon/types.h>

extern "C" {

	void platform_dputc(char c)
	{
		Genode::warning(__func__, " called with ", c);
	}

	void platform_dputs_thread(const char *str, size_t len)
	{
		Genode::warning(__func__, " called with (", len, ") ", Genode::Cstring(str));
	}

	int platform_dgetc(char *, bool)
	{
		return ZX_ERR_NOT_SUPPORTED;
	}

}
