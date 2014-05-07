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
#include <l4/re/c/namespace.h>
}

using namespace Fiasco;

extern "C" {

	long l4re_ns_query_srv(l4re_namespace_t srv, char const *name,
	                       l4_cap_idx_t const cap)
	{
		PDBG("srv=%lx name=%s cap=%lx", srv, name, cap);
		PWRN("%s: Not implemented yet!", __func__);
		return 0;
	}

}
