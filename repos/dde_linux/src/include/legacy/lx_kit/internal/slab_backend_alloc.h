
/*
 * \brief  Back-end allocator for Genode's slab allocator
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__INTERAL__SLAB_BACKEND_ALLOC_H_
#define _LX_KIT__INTERAL__SLAB_BACKEND_ALLOC_H_

/* Genode includes */
#include <base/allocator_avl.h>

/* Linux emulation environment includes */
#include <legacy/lx_kit/types.h>


namespace Lx {

	using Lx_kit::addr_t;

	class Slab_backend_alloc;
}


class Lx::Slab_backend_alloc : public Genode::Allocator
{
	public:

		/**
		 * Allocate
		 */
		virtual Alloc_result try_alloc(Genode::size_t size) = 0;
		virtual void free(void *addr) = 0;

		/**
		 * Return phys address for given virtual addr.
		 */
		virtual addr_t phys_addr(addr_t addr) = 0;

		/**
		 * Translate given physical address to virtual address
		 *
		 * \return virtual address, or 0 if no translation exists
		 */
		virtual addr_t virt_addr(addr_t phys) = 0;

		virtual addr_t start() const = 0;
		virtual addr_t end()   const = 0;

		static Slab_backend_alloc &mem();
		static Slab_backend_alloc &dma();
};

#endif /* _LX_KIT__INTERAL__SLAB_BACKEND_ALLOC_H_ */
