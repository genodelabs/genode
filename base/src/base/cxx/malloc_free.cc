/*
 * \brief  Simplistic malloc and free implementation
 * \author Norman Feske
 * \date   2006-07-21
 *
 * Malloc and free are required by the C++ exception handling.
 * Therefore, we provide it here.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <util/string.h>

using namespace Genode;

typedef unsigned long Block_header;


extern "C" void *malloc(unsigned size)
{
	/* enforce size to be a multiple of 4 bytes */
	size = (size + 3) & ~3;

	/*
	 * We store the size of the allocation at the very
	 * beginning of the allocated block and return
	 * the subsequent address. This way, we can retrieve
	 * the size information when freeing the block.
	 */
	unsigned long real_size = size + sizeof(Block_header);
	void *addr = 0;
	if (!Genode::env()->heap()->alloc(real_size, &addr))
		return 0;

	*(Block_header *)addr = real_size;
	return (Block_header *)addr + 1;
}


extern "C" void *calloc(unsigned nmemb, unsigned size)
{
	void *addr = malloc(nmemb*size);
	Genode::memset(addr, 0, nmemb*size);
	return addr;
}


extern "C" void free(void *ptr)
{
	if (!ptr) return;

	unsigned long *addr = ((unsigned long *)ptr) - 1;
	Genode::env()->heap()->free(addr, *addr);
}


extern "C" void *realloc(void *ptr, Genode::size_t size)
{
	if (!ptr)
		return malloc(size);

	if (!size) {
		free(ptr);
		return 0;
	}

	/* determine size of old block content (without header) */
	unsigned long old_size = *((Block_header *)ptr - 1)
	                         - sizeof(Block_header);

	/* do not reallocate if new size is less than the current size */
	if (size <= old_size)
		return ptr;

	/* allocate new block */
	void *new_addr = malloc(size);

	/* copy content from old block into new block */
	if (new_addr)
		memcpy(new_addr, ptr, Genode::min(old_size, (unsigned long)size));

	/* free old block */
	free(ptr);

	return new_addr;
}
