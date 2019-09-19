/*
 * \brief  Heap content copied from parent via 'fork'
 * \author Norman Feske
 * \date   2019-08-14
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__CLONED_MALLOC_HEAP_RANGE_H_
#define _LIBC__INTERNAL__CLONED_MALLOC_HEAP_RANGE_H_

/* Genode includes */
#include <region_map/region_map.h>

/* libc-internal includes */
#include <internal/clone_session.h>

namespace Libc { struct Cloned_malloc_heap_range; }


struct Libc::Cloned_malloc_heap_range
{
	Ram_allocator &ram;
	Region_map    &rm;

	Ram_dataspace_capability ds;

	size_t const size;
	addr_t const local_addr;

	Cloned_malloc_heap_range(Ram_allocator &ram, Region_map &rm,
	                         void *start, size_t size)
	try :
		ram(ram), rm(rm), ds(ram.alloc(size)), size(size),
		local_addr(rm.attach_at(ds, (addr_t)start))
	{ }
	catch (Region_map::Region_conflict) {
		error("could not clone heap region ", Hex_range(local_addr, size));
		throw;
	}

	void import_content(Clone_connection &clone_connection)
	{
		clone_connection.memory_content((void *)local_addr, size);
	}

	virtual ~Cloned_malloc_heap_range()
	{
		rm.detach(local_addr);
		ram.free(ds);
	}
};

#endif /* _LIBC__INTERNAL__CLONED_MALLOC_HEAP_RANGE_H_ */
