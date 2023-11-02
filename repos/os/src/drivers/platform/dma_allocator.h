/*
 * \brief  Platform driver - DMA allocator
 * \author Johannes Schlatow
 * \date   2023-03-23
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__DMA_ALLOCATOR_H_
#define _SRC__DRIVERS__PLATFORM__DMA_ALLOCATOR_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/registry.h>

namespace Driver {
	using namespace Genode;

	struct Dma_buffer;
	class Dma_allocator;
}


struct Driver::Dma_buffer : Registry<Dma_buffer>::Element
{
	Ram_dataspace_capability const cap;
	addr_t                         dma_addr;
	addr_t                         phys_addr;
	size_t                         size;
	Dma_allocator                & dma_alloc;

	Dma_buffer(Registry<Dma_buffer>         & registry,
	           Dma_allocator                & dma_alloc,
	           Ram_dataspace_capability const cap,
	           addr_t                         dma_addr,
	           size_t                         size,
	           addr_t                         phys_addr)
	: Registry<Dma_buffer>::Element(registry, *this),
	  cap(cap), dma_addr(dma_addr), phys_addr(phys_addr),
	  size(size), dma_alloc(dma_alloc)
	{ }

	~Dma_buffer();
};


class Driver::Dma_allocator
{
	public:

		struct Out_of_virtual_memory : Exception { };

	private:

		friend class Dma_buffer;

		Allocator          & _md_alloc;
		bool                 _remapping;
		bool const           _use_guard_page { true };
		Allocator_avl        _dma_alloc { &_md_alloc };
		Registry<Dma_buffer> _registry  { };

		addr_t _alloc_dma_addr(addr_t phys_addr, size_t size, bool const force_phys_addr);
		void   _free_dma_addr(addr_t dma_addr);

	public:

		void enable_remapping() { _remapping = true; }
		bool remapping()        { return _remapping; }

		bool reserve(addr_t phys_addr, size_t size);
		void unreserve(addr_t phys_addr, size_t size);

		Dma_buffer & alloc_buffer(Ram_dataspace_capability cap, addr_t phys_addr, size_t size);

		Registry<Dma_buffer>       & buffer_registry()       { return _registry; }
		Registry<Dma_buffer> const & buffer_registry() const { return _registry; }

		Dma_allocator(Allocator & md_alloc, bool const remapping);
};

#endif /* _SRC__DRIVERS__PLATFORM__DMA_ALLOCATOR_H */
