/*
 * \brief  Core-internal RAM-dataspace factory
 * \author Norman Feske
 * \date   2006-05-19
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <ram_dataspace_factory.h>

using namespace Core;


Ram_dataspace_factory::Alloc_ram_result
Ram_dataspace_factory::alloc_ram(size_t ds_size, Cache cache)
{
	using Range_allocation = Range_allocator::Allocation;

	/* zero-sized dataspaces are not allowed */
	if (!ds_size)
			return Alloc_ram_error::DENIED;

	/* dataspace allocation granularity is page size */
	ds_size = align_addr(ds_size, 12);

	/*
	 * Allocate physical backing store
	 *
	 * As an optimization for the use of large mapping sizes, we try to
	 * align the dataspace in physical memory naturally (size-aligned).
	 * If this does not work, we subsequently weaken the alignment constraint
	 * until the allocation succeeds.
	 */
	Range_allocator::Alloc_result allocated_range = Alloc_error::DENIED;

	/* apply constraints */
	if (_phys_range.start != 0 || _phys_range.end != ~0UL) {
		for (size_t align_log2 = log2(ds_size); align_log2 >= 12; align_log2--) {
			allocated_range = _phys_alloc.alloc_aligned(ds_size, (unsigned)align_log2, _phys_range);
			if (allocated_range.ok())
				break;
		}
	}

	/*
	 * If no physical constraint exists or constraints failed, try to allocate
	 * physical memory at high locations (3G for 32-bit / 4G for 64-bit platforms)
	 * in order to preserve lower physical regions for device drivers, which may
	 * have DMA constraints.
	 */
	if (!allocated_range.ok()) {
		addr_t     const high_start = (sizeof(void *) == 4 ? 3UL : 4UL) << 30;
		Phys_range const range { .start = high_start, .end = ~0UL };

		for (size_t align_log2 = log2(ds_size); align_log2 >= 12; align_log2--) {
			allocated_range = _phys_alloc.alloc_aligned(ds_size, (unsigned)align_log2, range);
			if (allocated_range.ok())
				break;
		}
	}

	/* retry if larger non-constrained memory allocation failed */
	if (!allocated_range.ok()) {
		for (size_t align_log2 = log2(ds_size); align_log2 >= 12; align_log2--) {
			allocated_range = _phys_alloc.alloc_aligned(ds_size, (unsigned)align_log2, _phys_range);
			if (allocated_range.ok())
				break;
		}
	}

	/*
	 * Normally, init's quota equals the size of physical memory and this quota
	 * is distributed among the processes. As we check the quota before
	 * allocating, the allocation should always succeed in theory. However,
	 * fragmentation could cause a failing allocation.
	 */
	if (allocated_range.failed()) {
		error("out of physical memory while allocating ", ds_size, " bytes ",
		      "in range [", Hex(_phys_range.start), "-", Hex(_phys_range.end), "]");

		if (allocated_range == Alloc_error::OUT_OF_RAM)
			return Alloc_ram_error::OUT_OF_RAM;

		if (allocated_range == Alloc_error::OUT_OF_CAPS)
			return Alloc_ram_error::OUT_OF_CAPS;

		return Alloc_ram_error::DENIED;
	}

	/*
	 * For non-cached RAM dataspaces, we mark the dataspace as write
	 * combined and expect the pager to evaluate this dataspace property
	 * when resolving page faults.
	 */
	Dataspace_component *ds_ptr = nullptr;
	try {
		allocated_range.with_result(
			[&] (Range_allocation &range) {
				ds_ptr = new (_ds_slab)
					Dataspace_component(ds_size, (addr_t)range.ptr,
					                    cache, true, this); },
			[] (Alloc_error) { });
	}
	catch (Out_of_ram)  { return Alloc_ram_error::OUT_OF_RAM; }
	catch (Out_of_caps) { return Alloc_ram_error::OUT_OF_CAPS; }
	catch (...)         { return Alloc_ram_error::DENIED; }

	Dataspace_component &ds = *ds_ptr;

	/* create native shared memory representation of dataspace */
	if (!_export_ram_ds(ds)) {
		warning("could not export RAM dataspace of size ", ds.size());

		/* cleanup unneeded resources */
		destroy(_ds_slab, &ds);
		return Alloc_ram_error::DENIED;
	}

	/*
	 * Fill new dataspaces with zeros. For non-cached RAM dataspaces, this
	 * function must also make sure to flush all cache lines related to the
	 * address range used by the dataspace.
	 */
	_clear_ds(ds);

	Dataspace_capability ds_cap = _ep.manage(&ds);

	allocated_range.with_result(
		[&] (Range_allocation &a) { a.deallocate = false; },
		[]  (Alloc_error) { });

	return static_cap_cast<Ram_dataspace>(ds_cap);
}


void Ram_dataspace_factory::free_ram(Ram_dataspace_capability ds_cap)
{
	Dataspace_component *ds = nullptr;
	_ep.apply(ds_cap, [&] (Dataspace_component *c)
	{
		if (!c) return;
		if (!c->owner(*this)) return;

		ds = c;

		size_t const ds_size = ds->size();

		/* tell entry point to forget the dataspace */
		_ep.dissolve(ds);

		/* remove dataspace from all RM sessions */
		ds->detach_from_rm_sessions();

		/* destroy native shared memory representation */
		_revoke_ram_ds(*ds);

		/* free physical memory that was backing the dataspace */
		_phys_alloc.free((void *)ds->phys_addr(), ds_size);
	});

	/* call dataspace destructor and free memory */
	if (ds)
		destroy(_ds_slab, ds);
}


size_t Ram_dataspace_factory::ram_size(Ram_dataspace_capability ds_cap)
{
	size_t result = 0;
	_ep.apply(ds_cap, [&] (Dataspace_component *c) {
		if (c && c->owner(*this))
			result = c->size(); });

	return result;
}


addr_t Ram_dataspace_factory::dataspace_dma_addr(Ram_dataspace_capability ds_cap)
{
	addr_t result = 0;
	_ep.apply(ds_cap, [&] (Dataspace_component *c) {
		if (c && c->owner(*this))
			result = c->phys_addr(); });

	return result;
}
