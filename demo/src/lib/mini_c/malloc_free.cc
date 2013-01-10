/*
 * \brief  Mini C malloc() and free()
 * \author Norman Feske
 * \date   2006-07-21
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


extern "C" void *malloc(unsigned size)
{
	/*
	 * We store the size of the allocation at the very
	 * beginning of the allocated block and return
	 * the subsequent address. This way, we can retrieve
	 * the size information when freeing the block.
	 */
	unsigned long real_size = size + sizeof(unsigned long);
	void *addr = 0;
	if (!env()->heap()->alloc(real_size, &addr))
		return 0;

	*(unsigned long *)addr = real_size;
	return (unsigned long *)addr + 1;
}


extern "C" void *calloc(unsigned nmemb, unsigned size)
{
	void *addr = malloc(nmemb*size);
	memset(addr, 0, nmemb*size);
	return addr;
}


extern "C" void free(void *ptr)
{
	unsigned long *addr = ((unsigned long *)ptr) - 1;
	env()->heap()->free(addr, *addr);
}
