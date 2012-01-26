/*
 * \brief  New and delete are special
 * \author Norman Feske
 * \date   2006-04-07
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/allocator.h>


void *operator new(Genode::size_t size, Genode::Allocator *allocator)
{
	if (!allocator)
		throw Genode::Allocator::Out_of_memory();

	return allocator->alloc(size);
}

void *operator new [] (Genode::size_t size, Genode::Allocator *allocator)
{
	if (!allocator)
		throw Genode::Allocator::Out_of_memory();

	return allocator->alloc(size);
}


void operator delete (void *ptr, Genode::Allocator *alloc)
{
	/*
	 * Warn on the attempt to use an allocator that relies on the size
	 * argument.
	 */
	if (alloc->need_size_for_free())
		PERR("C++ runtime: delete called with unsafe allocator, leaking memory");
}

