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
#include <base/log.h>
#include <foc/capability_space.h>

namespace Fiasco {
#include <l4/re/c/util/cap.h>
#include <l4/re/c/util/cap_alloc.h>
}

using namespace Fiasco;

extern "C" {

	l4_cap_idx_t l4re_util_cap_alloc(void)
	{
		l4_cap_idx_t ret = Genode::Capability_space::alloc_kcap();

		return ret;
	}


	void l4re_util_cap_free(l4_cap_idx_t cap)
	{
		Genode::warning(__func__, " not implemented");
	}


	l4_msgtag_t l4re_util_cap_release(l4_cap_idx_t cap)
	{
		l4_msgtag_t ret = l4_msgtag(0, 0, 0, 0);
		Genode::warning(__func__, " not implemented");
		return ret;
	}

}
