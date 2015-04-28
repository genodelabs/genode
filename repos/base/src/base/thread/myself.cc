/*
 * \brief  Implementation of the Thread API (generic myself())
 * \author Norman Feske
 * \date   2015-04-28
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>


Genode::Thread_base *Genode::Thread_base::myself()
{
	int dummy = 0; /* used for determining the stack pointer */

	/*
	 * If the stack pointer is outside the thread-context area, we assume that
	 * we are the main thread because this condition can never met by any other
	 * thread.
	 */
	addr_t sp = (addr_t)(&dummy);
	if (sp <  Native_config::context_area_virtual_base() ||
	    sp >= Native_config::context_area_virtual_base() +
	          Native_config::context_area_virtual_size())
		return 0;

	addr_t base = Context_allocator::addr_to_base(&dummy);
	return Context_allocator::base_to_context(base)->thread_base;
}
