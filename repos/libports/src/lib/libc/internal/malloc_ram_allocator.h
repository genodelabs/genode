/*
 * \brief  Utility for tracking the allocation of dataspaces by the malloc heap
 * \author Norman Feske
 * \date   2019-08-20
 */

/*
 * Copyright (C) 2016-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__MALLOC_RAM_ALLOCATOR_H_
#define _LIBC__INTERNAL__MALLOC_RAM_ALLOCATOR_H_

/* Genode includes */
#include <base/registry.h>
#include <base/allocator.h>
#include <base/ram_allocator.h>

/* libc-internal includes */
#include <internal/types.h>

namespace Libc { struct Malloc_ram_allocator; }


struct Libc::Malloc_ram_allocator : Ram_allocator
{
	using Allocator = Genode::Allocator;

	Allocator     &_md_alloc;
	Ram_allocator &_ram;

	struct Dataspace
	{
		Ram_dataspace_capability cap;
		Dataspace(Ram_dataspace_capability cap) : cap(cap) { }
		virtual ~Dataspace() { }
	};

	Registry<Registered<Dataspace> > _dataspaces { };

	void _release(Registered<Dataspace> &ds)
	{
		_ram.free(ds.cap);
		destroy(_md_alloc, &ds);
	}

	Malloc_ram_allocator(Allocator &md_alloc, Ram_allocator &ram)
	: _md_alloc(md_alloc), _ram(ram) { }

	~Malloc_ram_allocator()
	{
		_dataspaces.for_each([&] (Registered<Dataspace> &ds) {
			_release(ds); });
	}

	Alloc_result try_alloc(size_t size, Cache cache) override
	{
		return _ram.try_alloc(size, cache).convert<Alloc_result>(

			[&] (Ram::Allocation &a) -> Result {
				new (_md_alloc) Registered<Dataspace>(_dataspaces, a.cap);
				a.deallocate = false;
				return { *this, { a.cap, size } };
			},

			[&] (Alloc_error error) -> Result {
				return error; });
	}

	void _free(Ram::Allocation &a) override
	{
		_dataspaces.for_each([&] (Registered<Dataspace> &ds) {
			if (a.cap == ds.cap)
				_release(ds); });
	}
};

#endif /* _LIBC__INTERNAL__MALLOC_RAM_ALLOCATOR_H_ */
