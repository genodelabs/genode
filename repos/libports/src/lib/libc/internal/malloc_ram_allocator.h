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

namespace Libc { struct Malloc_ram_allocator; }


struct Libc::Malloc_ram_allocator : Ram_allocator
{
	typedef Genode::Allocator Allocator;

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

	Ram_dataspace_capability alloc(size_t size, Cache_attribute cached) override
	{
		Ram_dataspace_capability cap = _ram.alloc(size, cached);
		new (_md_alloc) Registered<Dataspace>(_dataspaces, cap);
		return cap;
	}

	void free(Ram_dataspace_capability ds_cap) override
	{
		_dataspaces.for_each([&] (Registered<Dataspace> &ds) {
			if (ds_cap == ds.cap)
				_release(ds); });
	}

	size_t dataspace_size(Ram_dataspace_capability ds_cap) const override
	{
		return _ram.dataspace_size(ds_cap);
	}
};

#endif /* _LIBC__INTERNAL__MALLOC_RAM_ALLOCATOR_H_ */
