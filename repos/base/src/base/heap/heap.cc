/*
 * \brief  Implementation of Genode heap partition
 * \author Norman Feske
 * \date   2006-05-17
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <rm_session/rm_session.h>
#include <base/heap.h>
#include <base/lock.h>

using namespace Genode;


Heap::Dataspace_pool::~Dataspace_pool()
{
	/* free all ram_dataspaces */
	for (Dataspace *ds; (ds = first()); ) {

		/*
		 * read dataspace capability and modify _ds_list before detaching
		 * possible backing store for Dataspace - we rely on LIFO list
		 * manipulation here!
		 */

		Ram_dataspace_capability ds_cap = ds->cap;
		void *ds_local_addr             = ds->local_addr;

		remove(ds);

		/* have the destructor of the 'cap' member called */
		delete ds;

		rm_session->detach(ds_local_addr);
		ram_session->free(ds_cap);
	}
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
		ds_addr = _ds_pool.rm_session->attach(new_ds_cap);
	} catch (Ram_session::Alloc_failed) {
		PWRN("could not allocate new dataspace of size %zu", size);
		return 0;
	} catch (Rm_session::Attach_failed) {
		PWRN("could not attach dataspace");
		_ds_pool.ram_session->free(new_ds_cap);
		return 0;
	}

	if (enforce_separate_metadata) {

		/* allocate the Dataspace structure */
		if (_unsynchronized_alloc(sizeof(Heap::Dataspace), &ds_meta_data_addr) < 0) {
			PWRN("could not allocate dataspace meta data");
			return 0;
		}

	} else {

		/* add new local address range to our local allocator */
		_alloc.add_range((addr_t)ds_addr, size);

		/* allocate the Dataspace structure */
		if (_alloc.alloc_aligned(sizeof(Heap::Dataspace), &ds_meta_data_addr, 2).is_error()) {
			PWRN("could not allocate dataspace meta data - this should never happen");
			return 0;
		}

	}

	ds = new (ds_meta_data_addr) Heap::Dataspace(new_ds_cap, ds_addr, size);

	_ds_pool.insert(ds);

	return ds;
}


bool Heap::_try_local_alloc(size_t size, void **out_addr)
{
	if (_alloc.alloc_aligned(size, out_addr, 2).is_error())
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
			PWRN("could not allocate dataspace");
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
	 * ('Dataspace' structures, AVL nodes).
	 * Finally, we align the size to a 4K page.
	 */
	dataspace_size = size + META_DATA_SIZE;

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


void Heap::free(void *addr, size_t size)
{
	/* serialize access of heap functions */
	Lock::Guard lock_guard(_lock);

	if (size >= BIG_ALLOCATION_THRESHOLD) {

		Heap::Dataspace *ds;

		for (ds = _ds_pool.first(); ds; ds = ds->next())
			if (((addr_t)addr >= (addr_t)ds->local_addr) &&
			    ((addr_t)addr <= (addr_t)ds->local_addr + ds->size - 1))
				break;

		_ds_pool.remove(ds);
		_ds_pool.rm_session->detach(ds->local_addr);
		_ds_pool.ram_session->free(ds->cap);

		_quota_used -= ds->size;

		/* have the destructor of the 'cap' member called */
		delete ds;
		_alloc.free(ds);

	} else {

		/*
	 	 * forward request to our local allocator
	 	 */
		_alloc.free(addr, size);

		_quota_used -= size;

	}
}
