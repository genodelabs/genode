/*
 * \brief  Allocator for anonymous memory used by libc
 * \author Norman Feske
 * \date   2012-05-18
 */

#ifndef _LIBC_MEM_ALLOC_H_
#define _LIBC_MEM_ALLOC_H_

/* Genode includes */
#include <base/allocator.h>

namespace Libc {

	struct Mem_alloc
	{
		virtual void *alloc(Genode::size_t size, Genode::size_t align_log2) = 0;
		virtual void free(void *ptr) = 0;
	};

	/**
	 * Return singleton instance of the memory allocator
	 */
	Mem_alloc *mem_alloc();
}

#endif /* _LIBC_MEM_ALLOC_H_ */
