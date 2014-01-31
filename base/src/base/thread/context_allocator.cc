/*
 * \brief  Context-allocator implementation for the Genode Thread API
 * \author Norman Feske
 * \author Martin Stein
 * \date   2014-01-26
 */

/*
 * Copyright (C) 2010-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>

using namespace Genode;


Thread_base::Context *Thread_base::Context_allocator::base_to_context(addr_t base)
{
	addr_t result = base + Native_config::context_virtual_size()
	                - sizeof(Context);
	return reinterpret_cast<Context *>(result);
}


addr_t Thread_base::Context_allocator::addr_to_base(void *addr)
{
	return ((addr_t)addr) & ~(Native_config::context_virtual_size() - 1);
}


size_t Thread_base::Context_allocator::base_to_idx(addr_t base)
{
	/* the first context isn't managed through the indices */
	return ((base - Native_config::context_area_virtual_base()) /
	       Native_config::context_virtual_size()) - 1;
}


addr_t Thread_base::Context_allocator::idx_to_base(size_t idx)
{
	/* the first context isn't managed through the indices */
	return Native_config::context_area_virtual_base() +
	       (idx + 1) * Native_config::context_virtual_size();
}


Thread_base::Context *
Thread_base::Context_allocator::alloc(Thread_base *thread_base, bool main_thread)
{
	Lock::Guard _lock_guard(_threads_lock);
	try {
		addr_t base;
		if (main_thread) {
			/* the main-thread context isn't managed by '_alloc' */
			base = Native_config::context_area_virtual_base();
		} else {
			/* contexts besides main-thread context are managed by '_alloc' */
			base = idx_to_base(_alloc.alloc());
		}
		return base_to_context(base);
	} catch(Bit_allocator<MAX_THREADS>::Out_of_indices) {
		return 0;
	}
}


void Thread_base::Context_allocator::free(Context *context)
{
	Lock::Guard _lock_guard(_threads_lock);
	addr_t const base = addr_to_base(context);

	/* the main-thread context isn't managed by '_alloc' */
	if (base == Native_config::context_area_virtual_base()) { return; }

	/* contexts besides main-thread context are managed by '_alloc' */
	_alloc.free(base_to_idx(base));
}


Thread_base::Context_allocator *Thread_base::_context_allocator()
{
	static Context_allocator context_allocator_inst;
	return &context_allocator_inst;
}
