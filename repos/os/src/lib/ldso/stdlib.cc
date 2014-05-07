/*
 * \brief  libc standard library calls
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <stdlib.h>
#include <base/env.h>
#include <util/string.h>

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
	void *addr = 0;
	if (!Genode::env()->heap()->alloc(real_size, &addr))
		return 0;

	*(Block_header *)addr = real_size;
	return (Block_header *)addr + 1;
}

extern "C" void *calloc(size_t nmemb, size_t size)
{
	void *ptr = malloc(size * nmemb);

	if (ptr)
		Genode::memset(ptr, 0, size * nmemb);

	return ptr;
}

extern "C" void free(void *ptr)
{
	if (!ptr) return;

	unsigned long *addr = ((unsigned long *)ptr) - 1;
	Genode::env()->heap()->free(addr, *addr);
}

/*
 * We use getenv to configure ldso
 */
extern "C"  char *getenv(const char *name)
{
#ifdef DEBUG
	if (!Genode::strcmp("LD_DEBUG", name))
		return (char*)"1";
#endif

	if (!Genode::strcmp("LD_LIBRARY_PATH", name))
		return (char*)"/";
	return NULL;
}
