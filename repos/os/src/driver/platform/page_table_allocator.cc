/*
 * \brief  Expanding page table allocator
 * \author Johannes Schlatow
 * \author Stefan Kalkowski
 * \date   2023-10-18
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <page_table_allocator.h>

Driver::Page_table_allocator::Backing_store::~Backing_store()
{
	while (_tree.first()) {
		Element * e = _tree.first();
		_tree.remove(e);
		Genode::destroy(_md_alloc, e);
	}
}


void Driver::Page_table_allocator::Backing_store::grow()
{
	Element * e = new (_md_alloc) Element(_range_alloc, _ram_alloc,
	                                      _env.rm(), _env.pd(), _chunk_size);

	_tree.insert(e);

	/* double _chunk_size */
	_chunk_size = Genode::min(_chunk_size << 1, (size_t)MAX_CHUNK_SIZE);
}


Genode::addr_t Driver::Page_table_allocator::_alloc()
{
	Align const align { .log2 = Genode::log2(TABLE_SIZE, 0u) };

	Alloc_result result = _allocator.alloc_aligned(TABLE_SIZE, align);

	if (result.failed()) {

		_backing_store.grow();

		/* retry allocation */
		result = _allocator.alloc_aligned(TABLE_SIZE, align);
	}

	return result.convert<addr_t>(
		[&] (Allocator::Allocation &a)  -> addr_t {
			a.deallocate = false;
			return (addr_t)a.ptr; },
		[&] (Alloc_error) -> addr_t { throw Alloc_failed(); });
}
