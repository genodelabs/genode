/*
 * \brief  Mini C malloc() and free()
 * \author Norman Feske
 * \date   2006-07-21
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <util/string.h>
#include <mini_c/init.h>

using namespace Genode;

static Allocator *_alloc_ptr;
static Allocator &alloc()
{
	if (_alloc_ptr)
		return *_alloc_ptr;

	error("missing call of mini_c_init");
	class Missing_mini_c_init { };
	throw Missing_mini_c_init();
}


void mini_c_init(Allocator &alloc) { _alloc_ptr = &alloc; }


extern "C" void *malloc(size_t size)
{
	/*
	 * We store the size of the allocation at the very
	 * beginning of the allocated block and return
	 * the subsequent address. This way, we can retrieve
	 * the size information when freeing the block.
	 */
	unsigned long const real_size = size + sizeof(unsigned long);

	return alloc().try_alloc(real_size).convert<void *>(

		[&] (void *ptr) {

			*(unsigned long *)ptr = real_size;
			return (unsigned long *)ptr + 1; },

		[&] (Allocator::Alloc_error) {
			return nullptr; });
}


extern "C" void *calloc(size_t nmemb, size_t size)
{
	void *addr = malloc(nmemb*size);
	memset(addr, 0, nmemb*size);
	return addr;
}


extern "C" void free(void *ptr)
{
	unsigned long *addr = ((unsigned long *)ptr) - 1;
	alloc().free(addr, *addr);
}
