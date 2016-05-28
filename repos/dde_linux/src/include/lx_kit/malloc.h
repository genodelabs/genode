/*
 * \brief  Linux kernel memory allocator
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_KIT__MALLOC_H_
#define _LX_KIT__MALLOC_H_

/* Linux emulation environment includes */
#include <lx_kit/types.h>
#include <lx_kit/internal/slab_alloc.h>
#include <lx_kit/internal/slab_backend_alloc.h>


namespace Lx {

	class Malloc;
}


class Lx::Malloc
{
	public:

		enum { MAX_SIZE_LOG2 = 16 /* 64 KiB */ };

		/**
		 * Alloc in slabs
		 */
		virtual void *alloc(Genode::size_t size, int align = 0, Genode::addr_t *phys = 0) = 0;

		virtual void free(void const *a) = 0;

		virtual void *alloc_large(size_t size) = 0;

		virtual void free_large(void *ptr) = 0;

		virtual size_t size(void const *a) = 0;

		virtual Genode::addr_t phys_addr(void *a) = 0;

		virtual Genode::addr_t virt_addr(Genode::addr_t phys) = 0;

		/**
		 * Belongs given address to this allocator
		 */
		virtual bool inside(addr_t const addr) const = 0;

		static Malloc &mem();
		static Malloc &dma();
};

#endif /* _LX_KIT__MALLOC_H_ */
