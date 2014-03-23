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
	return (base - Native_config::context_area_virtual_base()) /
	       Native_config::context_virtual_size();
}


addr_t Thread_base::Context_allocator::idx_to_base(size_t idx)
{
	return Native_config::context_area_virtual_base() +
	       idx * Native_config::context_virtual_size();
}


Thread_base::Context *
Thread_base::Context_allocator::alloc(Thread_base *thread_base, bool main_thread)
{
	if (main_thread)
		/* the main-thread context is the first one */
		return base_to_context(Native_config::context_area_virtual_base());

	try {
		Lock::Guard _lock_guard(_threads_lock);
		return base_to_context(idx_to_base(_alloc.alloc()));
	} catch(Bit_allocator<MAX_THREADS>::Out_of_indices) {
		return 0;
	}
}


void Thread_base::Context_allocator::free(Context *context)
{
	addr_t const base = addr_to_base(context);

	Lock::Guard _lock_guard(_threads_lock);
	_alloc.free(base_to_idx(base));
}


Thread_base::Context_allocator *Thread_base::_context_allocator()
{
	static Context_allocator context_allocator_inst;
	return &context_allocator_inst;
}
