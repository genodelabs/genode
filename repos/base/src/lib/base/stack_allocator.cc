/*
 * \brief  Stack-allocator implementation for the Genode Thread API
 * \author Norman Feske
 * \author Martin Stein
 * \date   2014-01-26
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-internal includes */
#include <base/internal/stack_allocator.h>

using namespace Genode;


Stack *Stack_allocator::base_to_stack(addr_t base)
{
	addr_t result = base + stack_virtual_size()
	                - sizeof(Stack);
	return reinterpret_cast<Stack *>(result);
}


addr_t Stack_allocator::addr_to_base(void *addr)
{
	return ((addr_t)addr) & ~(stack_virtual_size() - 1);
}


size_t Stack_allocator::base_to_idx(addr_t base)
{
	return (base - stack_area_virtual_base()) / stack_virtual_size();
}


addr_t Stack_allocator::idx_to_base(size_t idx)
{
	return stack_area_virtual_base() + idx * stack_virtual_size();
}


Stack *
Stack_allocator::alloc(Thread *, bool main_thread)
{
	if (main_thread)
		/* the main-thread stack is the first one */
		return base_to_stack(stack_area_virtual_base());

	try {
		Mutex::Guard guard(_threads_mutex);
		return base_to_stack(idx_to_base(_alloc.alloc()));
	} catch(Bit_allocator<MAX_THREADS>::Out_of_indices) {
		return 0;
	}
}


void Stack_allocator::free(Stack *stack)
{
	addr_t const base = addr_to_base(stack);

	Mutex::Guard guard(_threads_mutex);
	_alloc.free(base_to_idx(base));
}


Stack_allocator &Stack_allocator::stack_allocator()
{
	static Stack_allocator inst;
	return inst;
}
