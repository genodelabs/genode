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
#include <base/lock.h>

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
	ram_session->free(ds_cap);
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


Heap::Dataspace *Heap::_allocate_dataspace(size_t size, bool enforce_separate_metadata)
{
	Ram_dataspace_capability new_ds_cap;
	void *ds_addr = 0;
	void *ds_meta_data_addr = 0;
	Heap::Dataspace *ds = 0;

	/* make new ram dataspace available at our local address space */
	try {
		new_ds_cap = _ds_pool.ram_session->alloc(size);
		ds_addr = _ds_pool.region_map->attach(new_ds_cap);
	} catch (Ram_session::Alloc_failed) {
		return 0;
	} catch (Region_map::Attach_failed) {
		warning("could not attach dataspace");
		_ds_pool.ram_session->free(new_ds_cap);
		return 0;
	}

	if (enforce_separate_metadata) {

		/* allocate the Dataspace structure */
		if (_unsynchronized_alloc(sizeof(Heap::Dataspace), &ds_meta_data_addr) < 0) {
			warning("could not allocate dataspace meta data");
			return 0;
		}

	} else {

		/* add new local address range to our local allocator */
		_alloc->add_range((addr_t)ds_addr, size);

		/* allocate the Dataspace structure */
		if (_alloc->alloc_aligned(sizeof(Heap::Dataspace), &ds_meta_data_addr, log2(sizeof(addr_t))).error()) {
			warning("could not allocate dataspace meta data - this should never happen");
			return 0;
		}

	}

	ds = construct_at<Dataspace>(ds_meta_data_addr, new_ds_cap, ds_addr, size);

	_ds_pool.insert(ds);

	return ds;
}


bool Heap::_try_local_alloc(size_t size, void **out_addr)
{
	if (_alloc->alloc_aligned(size, out_addr, log2(sizeof(addr_t))).error())
		return false;

	_quota_used += size;
	return true;
}


bool Heap::_unsynchronized_alloc(size_t size, void **out_addr)
{
	size_t dataspace_size;

	if (size >= BIG_ALLOCATION_THRESHOLD) {

		/*
		 * big allocation
		 *
		 * in this case, we allocate one dataspace without any meta data in it
		 * and return its local address without going through the allocator.
		 */

		/* align to 4K page */
		dataspace_size = align_addr(size, 12);

		Heap::Dataspace *ds = _allocate_dataspace(dataspace_size, true);

		if (!ds) {
			warning("could not allocate dataspace");
			return false;
		}

		_quota_used += ds->size;

		*out_addr = ds->local_addr;

		return true;
	}

	/* try allocation at our local allocator */
	if (_try_local_alloc(size, out_addr))
		return true;

	/*
	 * Calculate block size of needed backing store. The block must hold the
	 * requested 'size' and we add some space for meta data
	 * ('Dataspace' structures, AVL-node slab blocks).
	 * Finally, we align the size to a 4K page.
	 */
	dataspace_size = size + Allocator_avl::slab_block_size() + sizeof(Heap::Dataspace);

	/*
	 * '_chunk_size' is a multiple of 4K, so 'dataspace_size' becomes
	 * 4K-aligned, too.
	 */
	size_t const request_size = _chunk_size * sizeof(umword_t);

	if ((dataspace_size < request_size) &&
		_allocate_dataspace(request_size, false)) {

		/*
		 * Exponentially increase chunk size with each allocated chunk until
		 * we hit 'MAX_CHUNK_SIZE'.
		 */
		_chunk_size = min(2*_chunk_size, (size_t)MAX_CHUNK_SIZE);

	} else {

		/* align to 4K page */
		dataspace_size = align_addr(dataspace_size, 12);
		if (!_allocate_dataspace(dataspace_size, false))
			return false;
	}

	/* allocate originally requested block */
	return _try_local_alloc(size, out_addr);
}


bool Heap::alloc(size_t size, void **out_addr)
{
	/* serialize access of heap functions */
	Lock::Guard lock_guard(_lock);

	/* check requested allocation against quota limit */
	if (size + _quota_used > _quota_limit)
		return false;

	return _unsynchronized_alloc(size, out_addr);
}


void Heap::free(void *addr, size_t)
{
	/* serialize access of heap functions */
	Lock::Guard lock_guard(_lock);

	/* try to find the size in our local allocator */
	size_t const size = _alloc->size_at(addr);

	if (size != 0) {

		/* forward request to our local allocator */
		_alloc->free(addr, size);
		_quota_used -= size;
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
		warning("heap could not free memory block");
		return;
	}

	_ds_pool.remove_and_free(*ds);
	_alloc->free(ds);

	_quota_used -= ds->size;
}


Heap::Heap(Ram_session *ram_session,
           Region_map  *region_map,
           size_t       quota_limit,
           void        *static_addr,
           size_t       static_size)
:
	_alloc(nullptr),
	_ds_pool(ram_session, region_map),
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
