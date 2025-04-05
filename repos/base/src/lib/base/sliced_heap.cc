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


Sliced_heap::Sliced_heap(Ram_allocator &ram_alloc, Region_map &region_map)
:
	_ram_alloc(ram_alloc), _region_map(region_map)
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


Allocator::Alloc_result Sliced_heap::try_alloc(size_t requested_size)
{
	/* allocation includes space for block meta data and is page-aligned */
	size_t const size = align_addr(requested_size + sizeof(Block), 12);

	return _ram_alloc.try_alloc(size).convert<Alloc_result>(

		[&] (Ram::Allocation &allocation) -> Alloc_result {

			struct Attach_guard
			{
				Region_map &rm;
				Region_map::Range range { };
				bool keep = false;

				Attach_guard(Region_map &rm) : rm(rm) { }

				~Attach_guard() { if (!keep && range.start) rm.detach(range.start); }

			} attach_guard(_region_map);

			Region_map::Attr attr { };
			attr.writeable = true;
			Region_map::Attach_result const result = _region_map.attach(allocation.cap, attr);
			if (result.failed()) {
				using Error = Region_map::Attach_error;
				return result.convert<Alloc_error>(
					[&] (auto) /* never called */ { return Alloc_error::DENIED; },
					[&] (Error e) {
						switch (e) {
						case Error::OUT_OF_RAM:  return Alloc_error::OUT_OF_RAM;
						case Error::OUT_OF_CAPS: return Alloc_error::OUT_OF_CAPS;
						case Error::REGION_CONFLICT:   break;
						case Error::INVALID_DATASPACE: break;
						}
						return Alloc_error::DENIED;
					});
			}

			result.with_result(
				[&] (Region_map::Range range) { attach_guard.range = range; },
				[&] (auto) { /* handled above */ });

			/* serialize access to block list */
			Mutex::Guard guard(_mutex);

			Block * const block = construct_at<Block>((void *)attach_guard.range.start,
			                                          allocation.cap, size);
			_consumed += size;
			_blocks.insert(block);

			allocation.deallocate = false;
			attach_guard.keep = true;

			/* skip meta data prepended to the payload portion of the block */
			return { *this, { .ptr = block + 1, .num_bytes = requested_size } };
		},
		[&] (Ram::Error e) { return e; });
}


void Sliced_heap::free(void *addr, size_t)
{
	Ram_dataspace_capability ds_cap;
	void *local_addr = nullptr;
	size_t num_bytes = 0;
	{
		/* serialize access to block list */
		Mutex::Guard guard(_mutex);

		/*
		 * The 'addr' argument points to the payload. We use pointer
		 * arithmetics to determine the pointer to the block's meta data that
		 * is prepended to the payload.
		 */
		Block * const block = reinterpret_cast<Block *>(addr) - 1;

		_blocks.remove(block);
		_consumed -= block->size;

		ds_cap     = block->ds;
		local_addr = block;
		num_bytes  = block->size;

		/*
		 * Call destructor to properly destruct the dataspace capability
		 * member of the 'Block'.
		 */
		block->~Block();
	}

	_region_map.detach(addr_t(local_addr));

	{
		/* deallocate via '~Allocation' */
		Ram::Allocation { _ram_alloc, { ds_cap, num_bytes } };
	}
}


size_t Sliced_heap::overhead(size_t size) const
{
	return align_addr(size + sizeof(Block), 12) - size;
}
