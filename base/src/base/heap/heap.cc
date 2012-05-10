/*
 * \brief  Implementation of Genode heap partition
 * \author Norman Feske
 * \date   2006-05-17
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
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

		remove(ds);
		delete ds;
		_rm_session->detach(ds->local_addr);
		_ram_session->free(ds_cap);
	}
}


int Heap::Dataspace_pool::expand(size_t size, Range_allocator *alloc)
{
	Ram_dataspace_capability new_ds_cap;
	void *local_addr, *ds_addr = 0;

	/* make new ram dataspace available at our local address space */
	try {
		new_ds_cap = _ram_session->alloc(size);
		local_addr = _rm_session->attach(new_ds_cap);
	} catch (Ram_session::Alloc_failed) {
		return -2;
	} catch (Rm_session::Attach_failed) {
		_ram_session->free(new_ds_cap);
		return -3;
	}

	/* add new local address range to our local allocator */
	alloc->add_range((addr_t)local_addr, size);

	/* now that we have new backing store, allocate Dataspace structure */
	if (!alloc->alloc_aligned(sizeof(Dataspace), &ds_addr, 2)) {
		PWRN("could not allocate meta data - this should never happen");
		return -1;
	}

	/* add dataspace information to list of dataspaces */
	Dataspace *ds  = new (ds_addr) Dataspace(new_ds_cap, local_addr);
	insert(ds);

	return 0;
}


int Heap::quota_limit(size_t new_quota_limit)
{
	if (new_quota_limit < _quota_used) return -1;
	_quota_limit = new_quota_limit;
	return 0;
}


bool Heap::_try_local_alloc(size_t size, void **out_addr)
{
	if (!_alloc.alloc_aligned(size, out_addr, 2))
		return false;

	_quota_used += size;
	return true;
}


bool Heap::alloc(size_t size, void **out_addr)
{
	/* serialize access of heap functions */
	Lock::Guard lock_guard(_lock);

	/* check requested allocation against quota limit */
	if (size + _quota_used > _quota_limit)
		return false;

	/* try allocation at our local allocator */
	if (_try_local_alloc(size, out_addr))
		return true;

	/*
	 * Calculate block size of needed backing store. The block must hold the
	 * requested 'size' and a new Dataspace structure if the allocation above
	 * failed. Finally, we align the size to a 4K page.
	 */
	size_t request_size = size + 1024;

	if (request_size < _chunk_size*sizeof(umword_t)) {
		request_size = _chunk_size*sizeof(umword_t);

		/*
		 * Exponentially increase chunk size with each allocated chunk until
		 * we hit 'MAX_CHUNK_SIZE'.
		 */
		_chunk_size = min(2*_chunk_size, (size_t)MAX_CHUNK_SIZE);
	}

	if (_ds_pool.expand(align_addr(request_size, 12), &_alloc) < 0) {
		PWRN("could not expand dataspace pool");
		return 0;
	}

	/* allocate originally requested block */
	return _try_local_alloc(size, out_addr);
}


void Heap::free(void *addr, size_t size)
{
	/* serialize access of heap functions */
	Lock::Guard lock_guard(_lock);

	/* forward request to our local allocator */
	_alloc.free(addr, size);

	_quota_used -= size;

	/*
	 * We could check for completely unused dataspaces...
	 * Yes, we could...
	 */
}
