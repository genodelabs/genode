/*
 * \brief  Simplistic malloc and free implementation
 * \author Norman Feske
 * \date   2006-07-21
 *
 * Malloc and free are required by the C++ exception handling.
 * Therefore, we provide it here.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <util/string.h>
#include <util/reconstructible.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/unmanaged_singleton.h>

using namespace Genode;


static Heap *cxx_heap_ptr;


Heap &cxx_heap()
{
	class Cxx_heap_uninitialized : Exception { };
	if (!cxx_heap_ptr)
		throw Cxx_heap_uninitialized();

	return *cxx_heap_ptr;
}


/**
 * Return heap partition for the private use within the cxx library.
 *
 * For creating the exception object, the C++ runtime calls
 * '__cxa_allocate_exception', which, in turn, calls 'malloc'. The cxx library
 * uses a local implementation of 'malloc' using a dedicated heap instance.
 */
void Genode::init_cxx_heap(Ram_allocator &ram, Region_map &rm)
{
	/*
	 * Exception frames are small. Hence, a small static backing store suffices
	 * for the cxx heap partition in the normal case. The 'env.ram()' session
	 * is used only if the demand exceeds the capacity of the 'initial_block'.
	 */
	static char initial_block[1024*sizeof(long)];

	cxx_heap_ptr = unmanaged_singleton<Heap>(&ram, &rm, Heap::UNLIMITED,
	                                         initial_block, sizeof(initial_block));
}


typedef unsigned long Block_header;


extern "C" void *malloc(size_t size)
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

	void *addr = nullptr;
	cxx_heap().try_alloc(real_size).with_result(
		[&] (void *ptr) { addr = ptr; },
		[&] (Allocator::Alloc_error error) {
			Genode::error(__func__,
			              ": cxx_heap allocation failed with error ", (int)error); });
	if (!addr)
		return nullptr;

	*(Block_header *)addr = real_size;
	return (Block_header *)addr + 1;
}


extern "C" void *calloc(size_t nmemb, size_t size)
{
	void * const addr = malloc(nmemb*size);

	if (addr == nullptr)
		return nullptr;

	Genode::memset(addr, 0, nmemb*size);
	return addr;
}


extern "C" void free(void *ptr)
{
	if (!ptr) return;

	unsigned long *addr = ((unsigned long *)ptr) - 1;
	cxx_heap().free(addr, *addr);
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
