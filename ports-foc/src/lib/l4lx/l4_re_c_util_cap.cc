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
#include <base/cap_map.h>

namespace Fiasco {
#include <l4/re/c/util/cap.h>
#include <l4/re/c/util/cap_alloc.h>
}

using namespace Fiasco;

static const bool DEBUG = false;

extern "C" {

	l4_cap_idx_t l4re_util_cap_alloc(void)
	{
		l4_cap_idx_t ret = Genode::cap_idx_alloc()->alloc(1)->kcap();

		if (DEBUG)
			PDBG("ret=%lx", ret);

		return ret;
	}


	void l4re_util_cap_free(l4_cap_idx_t cap)
	{
		PWRN("%s: Not implemented yet!",__func__);
	}


	l4_msgtag_t l4re_util_cap_release(l4_cap_idx_t cap)
	{
		l4_msgtag_t ret = l4_msgtag(0, 0, 0, 0);
		PWRN("%s: Not implemented yet!",__func__);
		return ret;
	}

}
