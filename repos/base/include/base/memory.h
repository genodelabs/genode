/*
 * \brief  Interfaces for byte-wise local memory allocations
 * \author Norman Feske
 * \date   2025-04-05
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__MEMORY_H_
#define _INCLUDE__BASE__MEMORY_H_

#include <base/error.h>
#include <util/allocation.h>

namespace Genode::Memory { struct Constrained_allocator; }


/**
 * Allocator of bytes that reflects allocation errors
 */
struct Genode::Memory::Constrained_allocator : Interface, Noncopyable
{
	struct Attr
	{
		void *ptr; size_t num_bytes;

		void print(Output &out) const
		{
			Genode::print(out, "ptr=", ptr, " num_bytes=", num_bytes);
		}
	};

	using Error      = Alloc_error;
	using Allocation = Genode::Allocation<Constrained_allocator>;
	using Result     = Allocation::Attempt;

	/**
	 * Allocate memory block
	 */
	virtual Result try_alloc(size_t num_bytes) = 0;

	/**
	 * Release allocation
	 *
	 * \noapi
	 */
	virtual void _free(Allocation &) = 0;
};


namespace Genode::Memory {

	/* shortcut for the most commonly used type allocation */
	using Allocation = Constrained_allocator::Allocation;
}

#endif /* _INCLUDE__BASE__MEMORY_H_ */
