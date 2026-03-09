/*
 * \brief  Types used within depot-autopilot
 * \author Norman Feske
 * \date   2025-11-12
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

#include <util/list_model.h>
#include <base/registry.h>
#include <base/heap.h>

namespace Depot_autopilot {

	using namespace Genode;

	struct Clock
	{
		uint64_t ms;

		void print(Output &out) const
		{
			Genode::print(out, ms/1000, ".", (ms/100)%10, (ms/10)%10);
		}
	};

	struct Stats
	{
		unsigned failed, succeeded, skipped;

		Clock total_time;

		void print(Output &out) const
		{
			Genode::print(out, "succeeded: ", succeeded, " "
			                   "failed: ",    failed,    " "
			                   "skipped: ",   skipped);
		}
	};

	struct Exit { using Code = String<16>; Code code; };
}

#endif /* _TYPES_H_ */
