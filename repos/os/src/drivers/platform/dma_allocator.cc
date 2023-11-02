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

/* Genode includes */
#include <base/log.h>

/* local includes */
#include <dma_allocator.h>

using namespace Driver;

addr_t Dma_allocator::_alloc_dma_addr(addr_t const phys_addr,
                                      size_t const size,
                                      bool   const force_phys_addr)
{
	using Alloc_error = Allocator::Alloc_error;

	/*
	 * 1:1 mapping (allocate at specified range from DMA memory allocator)
	 */
	if (force_phys_addr || !_remapping) {
		return _dma_alloc.alloc_addr(size, phys_addr).convert<addr_t>(
			[&] (void *) -> addr_t { return phys_addr; },
			[&] (Alloc_error err) -> addr_t {
				switch (err) {
				case Alloc_error::OUT_OF_RAM:  throw Out_of_ram();
				case Alloc_error::OUT_OF_CAPS: throw Out_of_caps();
				case Alloc_error::DENIED:
					error("Could not attach DMA range at ",
					      Hex_range(phys_addr, size), " (error: ", err, ")");
					break;
				}
				return 0UL;
			});
	}

	/* natural size align (to some limit) for better IOMMU TLB usage */
	unsigned size_align_log2 = unsigned(log2(size));
	if (size_align_log2 < 12) /* 4 kB */
		size_align_log2 = 12;
	if (size_align_log2 > 24) /* 16 MB */
		size_align_log2 = 24;

	/* add guard page */
	size_t guarded_size = size;
	if (_use_guard_page)
		guarded_size += 0x1000; /* 4 kB */

	return _dma_alloc.alloc_aligned(guarded_size, size_align_log2).convert<addr_t>(
		[&] (void *ptr) { return (addr_t)ptr; },
		[&] (Alloc_error err) -> addr_t {
			switch (err) {
			case Alloc_error::OUT_OF_RAM:  throw Out_of_ram();
			case Alloc_error::OUT_OF_CAPS: throw Out_of_caps();
			case Alloc_error::DENIED:
				error("Could not allocate DMA area of size: ", size,
				      " alignment: ", size_align_log2,
				      " size with guard page: ", guarded_size,
				      " total avail: ", _dma_alloc.avail(),
				     " (error: ", err, ")");
				break;
			};
			return 0;
		});
}


bool Dma_allocator::reserve(addr_t phys_addr, size_t size)
{
	return _alloc_dma_addr(phys_addr, size, true) == phys_addr;
}


void Dma_allocator::unreserve(addr_t phys_addr, size_t) { _free_dma_addr(phys_addr); }


Dma_buffer & Dma_allocator::alloc_buffer(Ram_dataspace_capability cap,
                                         addr_t                   phys_addr,
                                         size_t                   size)
{
	addr_t dma_addr = _alloc_dma_addr(phys_addr, size, false);

	if (!dma_addr)
		throw Out_of_virtual_memory();

	try {
		return * new (_md_alloc) Dma_buffer(_registry, *this, cap, dma_addr, size,
		                                    phys_addr);
	} catch (Out_of_ram)  {
		_free_dma_addr(dma_addr);
		throw;
	} catch (Out_of_caps) {
		_free_dma_addr(dma_addr);
		throw;
	}
}


void Dma_allocator::_free_dma_addr(addr_t dma_addr)
{
	_dma_alloc.free((void *)dma_addr);
}


Dma_allocator::Dma_allocator(Allocator       & md_alloc,
                             bool        const remapping)
:
	_md_alloc(md_alloc), _remapping(remapping)
{
	/* 0x1000 - 4GB */
	enum { DMA_SIZE = 0xffffe000 };
	_dma_alloc.add_range(0x1000, DMA_SIZE);

	/*
	 * Interrupt address range is special handled and in general not
	 * usable for normal DMA translations, see chapter 3.15
	 * of "Intel Virtualization Technology for Directed I/O"
	 * (March 2023, Revision 4.1)
	 */
	enum { IRQ_RANGE_BASE = 0xfee00000u, IRQ_RANGE_SIZE = 0x100000 };
	_dma_alloc.remove_range(IRQ_RANGE_BASE, IRQ_RANGE_SIZE);
}


Dma_buffer::~Dma_buffer()
{
	dma_alloc._free_dma_addr(dma_addr);
}
