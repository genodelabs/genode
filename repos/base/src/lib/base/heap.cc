/*
 * \brief  Implementation of Genode heap partition
 * \author Norman Feske
 * \date   2006-05-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/construct_at.h>
#include <base/env.h>
#include <base/log.h>
#include <base/heap.h>

using namespace Genode;


namespace {

	enum {
		MIN_CHUNK_SIZE =   4*1024,  /* in machine words */
		MAX_CHUNK_SIZE = 256*1024,
		/*
		 * Allocation sizes >= this value are considered as big
		 * allocations, which get their own dataspace. In contrast
		 * to smaller allocations, this memory is released to
		 * the RAM session when 'free()' is called.
		 */
		BIG_ALLOCATION_THRESHOLD = 64*1024 /* in bytes */
	};
}


void Heap::Dataspace_pool::remove_and_free(Dataspace &ds)
{
	/*
	 * read dataspace capability and modify _ds_list before detaching
	 * possible backing store for Dataspace - we rely on LIFO list
	 * manipulation here!
	 */

	Ram_dataspace_capability ds_cap = ds.cap;
	void *ds_local_addr             = ds.local_addr;

	remove(&ds);

	/*
	 * Call 'Dataspace' destructor to properly release the RAM dataspace
	 * capabilities. Note that we don't free the 'Dataspace' object at the
	 * local allocator because this is already done by the 'Heap'
	 * destructor prior executing the 'Dataspace_pool' destructor.
	 */
	ds.~Dataspace();

	region_map->detach(ds_local_addr);
	ram_alloc->free(ds_cap);
}


Heap::Dataspace_pool::~Dataspace_pool()
{
	/* free all ram_dataspaces */
	for (Dataspace *ds; (ds = first()); )
		remove_and_free(*ds);
}


int Heap::quota_limit(size_t new_quota_limit)
{
	if (new_quota_limit < _quota_used) return -1;
	_quota_limit = new_quota_limit;
	return 0;
}


Heap::Alloc_ds_result
Heap::_allocate_dataspace(size_t size, bool enforce_separate_metadata)
{
	using Result = Alloc_ds_result;

	return _ds_pool.ram_alloc->try_alloc(size).convert<Result>(

		[&] (Ram_dataspace_capability ds_cap) -> Result {

			struct Alloc_guard
			{
				Ram_allocator &ram;
				Ram_dataspace_capability ds;
				bool keep = false;

				Alloc_guard(Ram_allocator &ram, Ram_dataspace_capability ds)
				: ram(ram), ds(ds) { }

				~Alloc_guard() { if (!keep) ram.free(ds); }

			} alloc_guard(*_ds_pool.ram_alloc, ds_cap);

			struct Attach_guard
			{
				Region_map &rm;
				struct { void *ptr = nullptr; };
				bool keep = false;

				Attach_guard(Region_map &rm) : rm(rm) { }

				~Attach_guard() { if (!keep && ptr) rm.detach(ptr); }

			} attach_guard(*_ds_pool.region_map);

			try {
				attach_guard.ptr = _ds_pool.region_map->attach(ds_cap);
			}
			catch (Out_of_ram)                    { return Alloc_error::OUT_OF_RAM; }
			catch (Out_of_caps)                   { return Alloc_error::OUT_OF_CAPS; }
			catch (Region_map::Invalid_dataspace) { return Alloc_error::DENIED; }
			catch (Region_map::Region_conflict)   { return Alloc_error::DENIED; }

			Alloc_result metadata = Alloc_error::DENIED;

			/* allocate the 'Dataspace' structure */
			if (enforce_separate_metadata) {
				metadata = _unsynchronized_alloc(sizeof(Heap::Dataspace));

			} else {

				/* add new local address range to our local allocator */
				_alloc->add_range((addr_t)attach_guard.ptr, size).with_result(
					[&] (Range_allocator::Range_ok) {
						metadata = _alloc->alloc_aligned(sizeof(Heap::Dataspace), log2(16U)); },
					[&] (Alloc_error error) {
						metadata = error; });
			}

			return metadata.convert<Result>(
				[&] (void *md_ptr) -> Result {
					Dataspace &ds = *construct_at<Dataspace>(md_ptr, ds_cap,
					                                         attach_guard.ptr, size);
					_ds_pool.insert(&ds);
					alloc_guard.keep = attach_guard.keep = true;
					return &ds;
				},
				[&] (Alloc_error error) {
					return error; });
		},
		[&] (Alloc_error error) {
			return error; });
}


Allocator::Alloc_result Heap::_try_local_alloc(size_t size)
{
	return _alloc->alloc_aligned(size, log2(16U)).convert<Alloc_result>(

		[&] (void *ptr) {
			_quota_used += size;
			return ptr; },

		[&] (Alloc_error error) {
			return error; });
}


Allocator::Alloc_result Heap::_unsynchronized_alloc(size_t size)
{
	if (size >= BIG_ALLOCATION_THRESHOLD) {

		/*
		 * big allocation
		 *
		 * In this case, we allocate one dataspace without any meta data in it
		 * and return its local address without going through the allocator.
		 */

		/* align to 4K page */
		size_t const dataspace_size = align_addr(size, 12);

		return _allocate_dataspace(dataspace_size, true).convert<Alloc_result>(

			[&] (Dataspace *ds_ptr) {
				_quota_used += ds_ptr->size;
				return ds_ptr->local_addr; },

			[&] (Alloc_error error) {
				return error; });
	}

	/* try allocation at our local allocator */
	{
		Alloc_result result = _try_local_alloc(size);
		if (result.ok())
			return result;
	}

	size_t dataspace_size = size
	                      + Allocator_avl::slab_block_size()
	                      + sizeof(Heap::Dataspace);
	/* align to 4K page */
	dataspace_size = align_addr(dataspace_size, 12);

	/*
	 * '_chunk_size' is a multiple of 4K, so 'dataspace_size' becomes
	 * 4K-aligned, too.
	 */
	size_t const request_size = _chunk_size * sizeof(umword_t);

	Alloc_ds_result result = Alloc_error::DENIED;

	if (dataspace_size < request_size) {

		result = _allocate_dataspace(request_size, false);
		if (result.ok()) {

			/*
			 * Exponentially increase chunk size with each allocated chunk until
			 * we hit 'MAX_CHUNK_SIZE'.
			 */
			_chunk_size = min(2*_chunk_size, (size_t)MAX_CHUNK_SIZE);
		}
	} else {
		result = _allocate_dataspace(dataspace_size, false);
	}

	if (result.failed())
		return result.convert<Alloc_result>(
			[&] (Dataspace *)       { return Alloc_error::DENIED; },
			[&] (Alloc_error error) { return error; });

	/* allocate originally requested block */
	return _try_local_alloc(size);
}


Allocator::Alloc_result Heap::try_alloc(size_t size)
{
	if (size == 0)
		error("attempt to allocate zero-size block from heap");

	/* serialize access of heap functions */
	Mutex::Guard guard(_mutex);

	/* check requested allocation against quota limit */
	if (size + _quota_used > _quota_limit)
		return Alloc_error::DENIED;

	return _unsynchronized_alloc(size);
}


void Heap::free(void *addr, size_t)
{
	/* serialize access of heap functions */
	Mutex::Guard guard(_mutex);

	using Size_at_error = Allocator_avl::Size_at_error;

	Allocator_avl::Size_at_result size_at_result = _alloc->size_at(addr);

	if (size_at_result.ok()) {
		/* forward request to our local allocator */
		size_at_result.with_result(
			[&] (size_t size) {
				/* forward request to our local allocator */
				_alloc->free(addr, size);
				_quota_used -= size;
			},
			[&] (Size_at_error) { });

		return;
	}

	if (size_at_result == Size_at_error::MISMATCHING_ADDR) {
		/* address was found in local allocator but is not a block start address */
		error("heap could not free memory block: given address ", addr,
				" is not a block start adress");
		return;
	}

	/*
	 * Block could not be found in local allocator. So it is either a big
	 * allocation or invalid address.
	 */

	Heap::Dataspace *ds = nullptr;
	for (ds = _ds_pool.first(); ds; ds = ds->next())
		if (((addr_t)addr >= (addr_t)ds->local_addr) &&
		    ((addr_t)addr <= (addr_t)ds->local_addr + ds->size - 1))
			break;

	if (!ds) {
		warning("heap could not free memory block: invalid address ", addr);
		return;
	}

	_quota_used -= ds->size;

	_ds_pool.remove_and_free(*ds);
	_alloc->free(ds);
}


Heap::Heap(Ram_allocator *ram_alloc,
           Region_map    *region_map,
           size_t         quota_limit,
           void          *static_addr,
           size_t         static_size)
:
	_alloc(nullptr),
	_ds_pool(ram_alloc, region_map),
	_quota_limit(quota_limit), _quota_used(0),
	_chunk_size(MIN_CHUNK_SIZE)
{
	if (static_addr)
		_alloc->add_range((addr_t)static_addr, static_size);
}


Heap::~Heap()
{
	/*
	 * Revert allocations of heap-internal 'Dataspace' objects. Otherwise, the
	 * subsequent destruction of the 'Allocator_avl' would detect those blocks
	 * as dangling allocations.
	 *
	 * Since no new allocations can occur at the destruction time of the
	 * 'Heap', it is safe to release the 'Dataspace' objects at the allocator
	 * yet still access them afterwards during the destruction of the
	 * 'Allocator_avl'.
	 */
	for (Heap::Dataspace *ds = _ds_pool.first(); ds; ds = ds->next())
		_alloc->free(ds, sizeof(Dataspace));

	/*
	 * Destruct 'Allocator_avl' before destructing the dataspace pool. This
	 * order is important because some dataspaces of the dataspace pool are
	 * used as backing store for the allocator's meta data. If we destroyed
	 * the object pool before the allocator, the subsequent attempt to destruct
	 * the allocator would access no-longer-present backing store.
	 */
	_alloc.destruct();
}
