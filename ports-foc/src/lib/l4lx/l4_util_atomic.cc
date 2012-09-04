/*
 * \brief  L4Re functions needed by L4Linux.
 * \author Stefan Kalkowski
 * \date   2012-08-31
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <cpu/atomic.h>

namespace Fiasco {
#include <l4/util/atomic.h>
}

using namespace Fiasco;

extern "C" {

	int l4util_cmpxchg(volatile l4_umword_t * dest,
	                   l4_umword_t cmp_val, l4_umword_t new_val)
	{
		return Genode::cmpxchg((volatile int*)dest, cmp_val, new_val);
	}

}
