/*
 * \brief  L4Re functions needed by L4Linux.
 * \author Stefan Kalkowski
 * \date   2011-04-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

namespace Fiasco {
#include <l4/re/c/debug.h>
}

using namespace Fiasco;

extern "C" {

	void l4re_debug_obj_debug(l4_cap_idx_t srv, unsigned long function)
	{
		PWRN("%s: Not implemented yet!",__func__);
	}

}
