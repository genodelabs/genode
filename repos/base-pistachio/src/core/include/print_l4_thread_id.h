/*
 * \brief  Pistachio-specific thread helper
 * \author Julian Stecklina
 * \date   2008-02-20
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PRINT_L4_THREAD_ID_H_
#define _CORE__INCLUDE__PRINT_L4_THREAD_ID_H_

#include <base/printf.h>

namespace Pistachio {
#include <l4/types.h>

	inline void print_l4_thread_id(L4_ThreadId_t t)
	{
		if (L4_IsLocalId(t)) {
			Genode::printf("THREAD (local) %02lx (raw %08lx)\n",
			               t.local.X.local_id, t.raw);

		} else if (L4_IsGlobalId(t)) {
			Genode::printf("THREAD (global) %02lx (version %lx) (raw %08lx)\n",
			                t.global.X.thread_no, t.global.X.version, t.raw);

		} else {
			const char *name;

			if (t == L4_nilthread) name = "nilthread";
			else if (t == L4_anythread) name = "anythread";
			else name = "???";

			Genode::printf("THREAD (%s)\n", name);
		}
	}
}

#endif /* _CORE__INCLUDE__PRINT_L4_THREAD_ID_H_ */
