/*
 * \brief  Pistachio-specific helper for printing thread IDs
 * \author Norman Feske
 * \date   2016-08-15
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PRINT_L4_THREAD_ID_H_
#define _CORE__INCLUDE__PRINT_L4_THREAD_ID_H_

/* core includes */
#include <types.h>

/* base-internal includes */
#include <base/internal/pistachio.h>

namespace Core { struct Formatted_tid; }


struct Core::Formatted_tid
{
	Pistachio::L4_ThreadId_t tid;

	explicit Formatted_tid(Pistachio::L4_ThreadId_t tid) : tid(tid) { }

	void print(Output &out) const
	{
		using namespace Pistachio;
		using Genode::print;

		if (L4_IsLocalId(tid)) {

			uint8_t const local_id = (uint8_t)tid.local.X.local_id;
			print(out, "THREAD (local) ",
			           Hex(local_id, Hex::OMIT_PREFIX, Hex::PAD), " "
			           "(raw ", Hex(tid.raw, Hex::OMIT_PREFIX, Hex::PAD), ")");

		} else if (L4_IsGlobalId(tid)) {
			uint8_t const global_id = (uint8_t)tid.global.X.thread_no;
			print(out, "THREAD (global) ",
			           Hex(global_id, Hex::OMIT_PREFIX, Hex::PAD), " "
			           "(version ", Hex(tid.global.X.version, Hex::OMIT_PREFIX), ") ",
			           "(raw ",     Hex(tid.raw, Hex::OMIT_PREFIX, Hex::PAD), ")");

		} else {
			const char *name;

			if      (tid == L4_nilthread) name = "nilthread";
			else if (tid == L4_anythread) name = "anythread";
			else                          name = "???";

			print(out, "THREAD (", name, ")");
		}
	}
};

#endif /* _CORE__INCLUDE__PRINT_L4_THREAD_ID_H_ */
