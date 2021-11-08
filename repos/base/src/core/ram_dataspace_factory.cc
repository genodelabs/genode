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

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <ram_dataspace_factory.h>

using namespace Genode;


Ram_allocator::Alloc_result
Ram_dataspace_factory::try_alloc(size_t ds_size, Cache cache)
{
	/* zero-sized dataspaces are not allowed */
	if (!ds_size)
			return Alloc_error::DENIED;

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
	void *ds_addr = nullptr;
	bool alloc_succeeded = false;

	/*
	 * If no physical constraint exists, try to allocate physical memory at
	 * high locations (3G for 32-bit / 4G for 64-bit platforms) in order to
	 * preserve lower physical regions for device drivers, which may have DMA
	 * constraints.
	 */
	if (_phys_range.start == 0 && _phys_range.end == ~0UL) {

		addr_t     const high_start = (sizeof(void *) == 4 ? 3UL : 4UL) << 30;
		Phys_range const range { .start = high_start, .end = _phys_range.end };

		for (size_t align_log2 = log2(ds_size); align_log2 >= 12; align_log2--) {
			if (_phys_alloc.alloc_aligned(ds_size, &ds_addr, align_log2, range).ok()) {
				alloc_succeeded = true;
				break;
			}
		}
	}

	/* apply constraints, or retry if larger memory allocation failed */
	if (!alloc_succeeded) {
		for (size_t align_log2 = log2(ds_size); align_log2 >= 12; align_log2--) {
			if (_phys_alloc.alloc_aligned(ds_size, &ds_addr, align_log2,
			                              _phys_range).ok()) {
				alloc_succeeded = true;
				break;
			}
		}
	}

	/*
	 * Helper to release the allocated physical memory whenever we leave the
	 * scope via an exception.
	 */
	class Phys_alloc_guard
	{
		private:

			/*
			 * Noncopyable
			 */
			Phys_alloc_guard(Phys_alloc_guard const &);
			Phys_alloc_guard &operator = (Phys_alloc_guard const &);

		public:

			Range_allocator &phys_alloc;
			void * const ds_addr;
			bool ack = false;

			Phys_alloc_guard(Range_allocator &phys_alloc, void *ds_addr)
			: phys_alloc(phys_alloc), ds_addr(ds_addr) { }

			~Phys_alloc_guard() { if (!ack) phys_alloc.free(ds_addr); }

	} phys_alloc_guard(_phys_alloc, ds_addr);

	/*
	 * Normally, init's quota equals the size of physical memory and this quota
	 * is distributed among the processes. As we check the quota before
	 * allocating, the allocation should always succeed in theory. However,
	 * fragmentation could cause a failing allocation.
	 */
	if (!alloc_succeeded) {
		error("out of physical memory while allocating ", ds_size, " bytes ",
		      "in range [", Hex(_phys_range.start), "-", Hex(_phys_range.end), "]");
		return Alloc_error::OUT_OF_RAM;
	}

	/*
	 * For non-cached RAM dataspaces, we mark the dataspace as write
	 * combined and expect the pager to evaluate this dataspace property
	 * when resolving page faults.
	 */
	Dataspace_component *ds_ptr = nullptr;
	try {
		ds_ptr = new (_ds_slab)
			Dataspace_component(ds_size, (addr_t)ds_addr, cache, true, this);
	}
	catch (Out_of_ram)  { return Alloc_error::OUT_OF_RAM; }
	catch (Out_of_caps) { return Alloc_error::OUT_OF_CAPS; }
	catch (...)         { return Alloc_error::DENIED; }

	Dataspace_component &ds = *ds_ptr;

	/* create native shared memory representation of dataspace */
	try { _export_ram_ds(ds); }
	catch (Core_virtual_memory_exhausted) {
		warning("could not export RAM dataspace of size ", ds.size());

		/* cleanup unneeded resources */
		destroy(_ds_slab, &ds);
		return Alloc_error::DENIED;
	}

	/*
	 * Fill new dataspaces with zeros. For non-cached RAM dataspaces, this
	 * function must also make sure to flush all cache lines related to the
	 * address range used by the dataspace.
	 */
	_clear_ds(ds);

	Dataspace_capability ds_cap = _ep.manage(&ds);

	phys_alloc_guard.ack = true;

	return static_cap_cast<Ram_dataspace>(ds_cap);
}


void Ram_dataspace_factory::free(Ram_dataspace_capability ds_cap)
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


size_t Ram_dataspace_factory::dataspace_size(Ram_dataspace_capability ds_cap) const
{
	size_t result = 0;
	_ep.apply(ds_cap, [&] (Dataspace_component *c) {
		if (c && c->owner(*this))
			result = c->size(); });

	return result;
}
