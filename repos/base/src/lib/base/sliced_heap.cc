/*
 * \brief  Heap that stores each block at a separate dataspace
 * \author Norman Feske
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/construct_at.h>
#include <base/heap.h>
#include <base/log.h>

using namespace Genode;


Sliced_heap::Sliced_heap(Ram_session &ram_session, Region_map &region_map)
:
	_ram_session(ram_session), _region_map(region_map), _consumed(0)
{ }


Sliced_heap::~Sliced_heap()
{
	for (Block *b; (b = _blocks.first()); ) {
		/*
		 * Compute pointer to payload, which follows the meta-data header.
		 * Note the pointer arithmetics. By adding 1 to 'b', we end up with
		 * 'payload' pointing to the data portion of the block.
		 */
		void * const payload = b + 1;
		free(payload, b->size);
	}
}


bool Sliced_heap::alloc(size_t size, void **out_addr)
{
	/* allocation includes space for block meta data and is page-aligned */
	size = align_addr(size + sizeof(Block), 12);

	Ram_dataspace_capability ds_cap;
	Block *block = nullptr;

	try {
		ds_cap = _ram_session.alloc(size);
		block  = _region_map.attach(ds_cap);
	} catch (Region_map::Attach_failed) {
		error("could not attach dataspace to local address space");
		_ram_session.free(ds_cap);
		return false;
	} catch (Ram_session::Alloc_failed) {
		error("could not allocate dataspace with size ", size);
		return false;
	}

	/* serialize access to block list */
	Lock::Guard lock_guard(_lock);

	construct_at<Block>(block, ds_cap, size);

	_consumed += size;
	_blocks.insert(block);

	/* skip meta data prepended to the payload portion of the block */
	*out_addr = block + 1;

	return true;
}


void Sliced_heap::free(void *addr, size_t size)
{
	Ram_dataspace_capability ds_cap;
	void *local_addr = nullptr;
	{
		/* serialize access to block list */
		Lock::Guard lock_guard(_lock);

		/*
		 * The 'addr' argument points to the payload. We use pointer
		 * arithmetics to determine the pointer to the block's meta data that
		 * is prepended to the payload.
		 */
		Block * const block = reinterpret_cast<Block *>(addr) - 1;

		_blocks.remove(block);
		_consumed -= block->size;
		ds_cap = block->ds;
		local_addr = block;

		/*
		 * Call destructor to properly destruct the dataspace capability
		 * member of the 'Block'.
		 */
		block->~Block();
	}

	_region_map.detach(local_addr);
	_ram_session.free(ds_cap);
}


size_t Sliced_heap::overhead(size_t size) const
{
	return align_addr(size + sizeof(Block), 12) - size;
}
