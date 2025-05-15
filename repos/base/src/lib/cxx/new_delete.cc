/*
 * \brief  New and delete are special
 * \author Norman Feske
 * \date   2006-04-07
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/allocator.h>
#include <base/sleep.h>

/* C++ runtime includes */
#include <new>

using Genode::size_t;
using Genode::Allocator;
using Genode::Deallocator;
using Genode::sleep_forever;


static void *try_alloc(Allocator *alloc, size_t size)
{
	if (!alloc)
		Genode::raise(Genode::Alloc_error::DENIED);

	return alloc->alloc(size);
}


void *operator new    (__SIZE_TYPE__ s, Allocator *a) { return try_alloc(a, s); }
void *operator new [] (__SIZE_TYPE__ s, Allocator *a) { return try_alloc(a, s); }
void *operator new    (__SIZE_TYPE__ s, Allocator &a) { return a.alloc(s); }
void *operator new [] (__SIZE_TYPE__ s, Allocator &a) { return a.alloc(s); }


static void try_dealloc(void *ptr, Deallocator &dealloc)
{
	/*
	 * Log error and block on the attempt to use an allocator that relies on
	 * the size argument.
	 */
	if (dealloc.need_size_for_free()) {
		Genode::error("C++ runtime: delete called with allocator, which needs "
		              "'size' on free. Blocking before leaking memory...");
		sleep_forever();
	}

	/* size not required, so call with dummy size */
	dealloc.free(ptr, 0);
}


void operator delete (void *ptr, Deallocator *dealloc) { try_dealloc(ptr, *dealloc); }
void operator delete (void *ptr, Deallocator &dealloc) { try_dealloc(ptr,  dealloc); }
