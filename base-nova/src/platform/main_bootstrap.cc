/*
 * \brief  Platform-specific helper functions for the _main() function
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2009-12-28
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/cap_map.h>
#include <base/env.h>

#include <base/printf.h>

namespace Genode { void platform_main_bootstrap(); }

enum { CAP_RANGE_START = 4096 };

Genode::Cap_range * initial_range()
{
	static Genode::Cap_range range(CAP_RANGE_START);
	return &range;
}

extern "C" Genode::addr_t __initial_sp;

void Genode::platform_main_bootstrap()
{
	static struct Bootstrap
	{
		Bootstrap()
		{
			cap_map()->insert(initial_range());

			/* for Core we can't perform the following code so early */
			if (__initial_sp)
				return;

			unsigned index = initial_range()->base() + initial_range()->elements();

/*
			printf("initial selector range [0x%8lx:0x%8lx)\n",
			       initial_range()->base(), initial_range()->base() +
			       initial_range()->elements());
*/

			for (unsigned i = 0; i < 16; i++) {

				Ram_dataspace_capability ds = env()->ram_session()->alloc(4096);
				addr_t local = env()->rm_session()->attach(ds);

				Cap_range * range = reinterpret_cast<Cap_range *>(local);
				*range = Cap_range(index);

				cap_map()->insert(range);

/*
				printf("add cap selector range [0x%8lx:0x%8lx)\n",
				       range->base(), range->base() + range->elements());
*/

				index = range->base() + range->elements();
			}
		}
	} bootstrap;
}
