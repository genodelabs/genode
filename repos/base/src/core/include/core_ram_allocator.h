/*
 * \brief  RAM allocator for core-internal use
 * \author Norman Feske
 * \date   2025-04-01
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_RAM_ALLOCATOR_H_
#define _CORE__INCLUDE__CORE_RAM_ALLOCATOR_H_

/* Genode includes */
#include <base/ram_allocator.h>

/* core includes */
#include <ram_dataspace_factory.h>

namespace Core { struct Core_ram_allocator; }


struct Core::Core_ram_allocator : Ram_allocator
{
	Ram_dataspace_factory &_factory;

	Alloc_result try_alloc(size_t size, Cache cache) override
	{
		using Pd_error = Pd_session::Alloc_ram_error;
		return _factory.alloc_ram(size, cache).convert<Alloc_result>(
			[&] (Ram_dataspace_capability cap) { return cap; },
			[&] (Pd_error e) {
				switch (e) {
				case Pd_error::OUT_OF_CAPS: return Alloc_error::OUT_OF_CAPS;
				case Pd_error::OUT_OF_RAM: return Alloc_error::OUT_OF_RAM;;
				case Pd_error::DENIED:
					break;
				}
				return Alloc_error::DENIED;
			});
	}

	void free(Ram_dataspace_capability ds) override
	{
		_factory.free_ram(ds);
	}

	size_t dataspace_size(Ram_dataspace_capability ds) override
	{
		return _factory.ram_size(ds);
	}

	Core_ram_allocator(Ram_dataspace_factory &factory) : _factory(factory) { }
};

#endif /* _CORE__INCLUDE__CORE_RAM_ALLOCATOR_H_ */
