/*
 * \brief  Implementation of the Thread API (generic myself())
 * \author Norman Feske
 * \date   2015-04-28
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/stack_allocator.h>
#include <base/internal/stack_area.h>


Genode::Thread *Genode::Thread::myself()
{
	int dummy = 0; /* used for determining the stack pointer */

	/*
	 * If the stack pointer is outside the stack area, we assume that
	 * we are the main thread because this condition can never met by any other
	 * thread.
	 */
	addr_t sp = (addr_t)(&dummy);
	if (sp <  Genode::stack_area_virtual_base() ||
	    sp >= Genode::stack_area_virtual_base() + Genode::stack_area_virtual_size())
		return 0;

	addr_t base = Stack_allocator::addr_to_base(&dummy);
	return &Stack_allocator::base_to_stack(base)->thread();
}
