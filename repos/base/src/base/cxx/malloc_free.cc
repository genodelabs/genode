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
#include <base/heap.h>
#include <util/string.h>

using namespace Genode;


/**
 * Return heap partition for the private use within the cxx library.
 *
 * If we used the 'env()->heap()' with the C++ runtime, we would run into a
 * deadlock when a 'Ram_session::Alloc_failed' exception is thrown from within
 * 'Heap::alloc'. For creating the exception object, the C++ runtime calls
 * '__cxa_allocate_exception', which, in turn, calls 'malloc'. If our 'malloc'
 * implementation called 'env()->heap()->alloc()', we would end up in a
 * recursive attempt to obtain the 'env()->heap()' lock.
 *
 * By using a dedicated heap instance for the cxx library, we avoid this
 * circular condition.
 */
static Heap *cxx_heap()
{
	/*
	 * Exception frames are small (ca. 100 bytes). Hence, a small static
	 * backing store suffices for the cxx heap partition in the normal
	 * case. The 'env()->ram_session' is used only if the demand exceeds
	 * the capacity of the 'initial_block'.
	 */
	static char initial_block[512];
	static Heap heap(env()->ram_session(), env()->rm_session(),
	                 Heap::UNLIMITED, initial_block, sizeof(initial_block));
	return &heap;
}


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
	if (!cxx_heap()->alloc(real_size, &addr))
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
	cxx_heap()->free(addr, *addr);
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
