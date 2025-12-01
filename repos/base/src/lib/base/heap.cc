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
	void * const at   = ds.local_addr;
	size_t const size = ds.size;

	remove(&ds);

	/*
	 * Call 'Dataspace' destructor to properly release the RAM dataspace
	 * capabilities. Note that we don't free the 'Dataspace' object at the
	 * local allocator because this is already done by the 'Heap'
	 * destructor prior executing the 'Dataspace_pool' destructor.
	 */
	ds.~Dataspace();

	{
		/* detach via '~Attachment' */
		Local_rm::Attachment { *local_rm, { at, size } };

		/* deallocate via '~Allocation' */
		Ram::Allocation { *ram_alloc, { ds_cap, size } };
	}
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
		[&] (Ram::Allocation &allocation) -> Result {

			Local_rm::Attach_attr attr { };
			attr.writeable = true;
			return _ds_pool.local_rm->attach(allocation.cap, attr).convert<Result>(
				[&] (Local_rm::Attachment &attachment) -> Result {

					Alloc_result metadata = Alloc_error::DENIED;

					/* allocate the 'Dataspace' structure */
					if (enforce_separate_metadata) {
						metadata = _unsynchronized_alloc(sizeof(Heap::Dataspace));

					} else {

						/* add new local address range to our local allocator */
						_alloc->add_range(addr_t(attachment.ptr), size).with_result(
							[&] (Ok) {
								Align const AT_16_BYTES { .log2 = 4 };
								metadata = _alloc->alloc_aligned(sizeof(Heap::Dataspace),
								                                 AT_16_BYTES); },
							[&] (Alloc_error error) {
								metadata = error; });
					}

					return metadata.convert<Result>(
						[&] (Allocation &md) -> Result {
							md        .deallocate = false;
							allocation.deallocate = false;
							attachment.deallocate = false;

							Dataspace &ds = *construct_at<Dataspace>(md.ptr, allocation.cap,
							                                         attachment.ptr, size);
							_ds_pool.insert(&ds);
							return &ds;
						},
						[&] (Alloc_error error) {
							return error; });
				},
				[&] (Local_rm::Error e) {
					using Error = Local_rm::Error;
					switch (e) {
					case Error::OUT_OF_RAM:  return Alloc_error::OUT_OF_RAM;
					case Error::OUT_OF_CAPS: return Alloc_error::OUT_OF_CAPS;
					case Error::REGION_CONFLICT:   break;
					case Error::INVALID_DATASPACE: break;
					}
					return Alloc_error::DENIED;
				}
			);
		},
		[&] (Alloc_error error) {
			return error; });
}


Allocator::Alloc_result Heap::_try_local_alloc(size_t size)
{
	Align const AT_16_BYTES { .log2 = 4 };
	return _alloc->alloc_aligned(size, AT_16_BYTES).convert<Alloc_result>(

		[&] (Allocation &a) -> Alloc_result {
			a.deallocate = false;
			_quota_used += size;
			return { *this, { a.ptr, size } }; },

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
		size_t const dataspace_size = align_addr(size, AT_PAGE);

		return _allocate_dataspace(dataspace_size, true).convert<Alloc_result>(

			[&] (Dataspace *ds_ptr) -> Alloc_result {
				_quota_used += ds_ptr->size;
				return { *this, { ds_ptr->local_addr, size } }; },

			[&] (Alloc_error error) {
				return error; });
	}

	/* try allocation at our local allocator */
	{
		Alloc_result result = _try_local_alloc(size);
		if (result.ok())
			return result.convert<Alloc_result>(
				[&] (Allocation &a) -> Alloc_result {
					a.deallocate = false;
					return { *this, a }; },
				[&] (Alloc_error e) { return e; });
	}

	size_t dataspace_size = size
	                      + Allocator_avl::slab_block_size()
	                      + sizeof(Heap::Dataspace);
	/* align to 4K page */
	dataspace_size = align_addr(dataspace_size, AT_PAGE);

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
	_alloc->free(ds, ds->size);
}


Heap::Heap(Ram_allocator *ram_alloc,
           Local_rm      *local_rm,
           size_t         quota_limit,
           void          *static_addr,
           size_t         static_size)
:
	_alloc(nullptr),
	_ds_pool(ram_alloc, local_rm),
	_quota_limit(quota_limit), _quota_used(0),
	_chunk_size(MIN_CHUNK_SIZE)
{
	if (static_addr)
		if (_alloc->add_range((addr_t)static_addr, static_size).failed())
			warning("unable to add static range at heap-construction time");
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
