/*
 * \brief  Synchronized wrapper for the 'Ram_allocator' interface
 * \author Norman Feske
 * \date   2017-05-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SYNCED_RAM_ALLOCATOR_H_
#define _CORE__INCLUDE__SYNCED_RAM_ALLOCATOR_H_

/* Genode includes */
#include <base/ram_allocator.h>
#include <base/mutex.h>

namespace Genode { class Synced_ram_allocator; }


class Genode::Synced_ram_allocator : public Ram_allocator
{
	private:

		Mutex mutable _mutex { };

		Ram_allocator &_alloc;

	public:

		Synced_ram_allocator(Ram_allocator &alloc) : _alloc(alloc) { }

		Ram_dataspace_capability alloc(size_t size, Cache_attribute cached) override
		{
			Mutex::Guard mutex_guard(_mutex);
			return _alloc.alloc(size, cached);
		}

		void free(Ram_dataspace_capability ds) override
		{
			Mutex::Guard mutex_guard(_mutex);
			_alloc.free(ds);
		}

		size_t dataspace_size(Ram_dataspace_capability ds) const override
		{
			Mutex::Guard mutex_guard(_mutex);
			return _alloc.dataspace_size(ds);
		}
};

#endif /* _CORE__INCLUDE__SYNCED_RAM_ALLOCATOR_H_ */
