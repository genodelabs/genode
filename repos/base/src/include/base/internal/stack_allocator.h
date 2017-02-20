/*
 * \brief  Stack allocator
 * \author Norman Feske
 * \date   2006-04-28
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__STACK_ALLOCATOR_H_
#define _INCLUDE__BASE__INTERNAL__STACK_ALLOCATOR_H_

/* Genode includes */
#include <util/bit_allocator.h>

/* base-internal includes */
#include <base/internal/stack.h>
#include <base/internal/stack_area.h>

namespace Genode { class Stack_allocator; }


/**
 * Manage the allocation of stacks within the stack area
 *
 * There exists only one instance of this class per process.
 */
class Genode::Stack_allocator
{
	private:

		static constexpr size_t MAX_THREADS =
			stack_area_virtual_size() /
			stack_virtual_size();

		struct Stack_bit_allocator : Bit_allocator<MAX_THREADS>
		{
			Stack_bit_allocator()
			{
				/* the first index is used by main thread */
				_reserve(0, 1);
			}
		} _alloc;

		Lock _threads_lock;

	public:

		/**
		 * Allocate stack for specified thread
		 *
		 * \param thread       thread for which to allocate the new stack
		 * \param main_thread  wether to alloc for the main thread
		 *
		 * \return  virtual address of new stack, or
		 *          0 if the allocation failed
		 */
		Stack *alloc(Thread *thread, bool main_thread);

		/**
		 * Release stack
		 */
		void free(Stack *);

		/**
		 * Return 'Stack' object for a given base address
		 */
		static Stack *base_to_stack(addr_t base);

		/**
		 * Return base address of stack containing the specified address
		 */
		static addr_t addr_to_base(void *addr);

		/**
		 * Return index in stack area for a given base address
		 */
		static size_t base_to_idx(addr_t base);

		/**
		 * Return base address of stack given index in stack area
		 */
		static addr_t idx_to_base(size_t idx);

		/**
		 * Return singleton thread allocator
		 */
		static Stack_allocator &stack_allocator();
};


#endif /* _INCLUDE__BASE__INTERNAL__STACK_ALLOCATOR_H_ */
