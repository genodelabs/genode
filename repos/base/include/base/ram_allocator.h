/*
 * \brief  Interface for allocating RAM dataspaces
 * \author Norman Feske
 * \date   2017-05-02
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__RAM_ALLOCATOR_H_
#define _INCLUDE__BASE__RAM_ALLOCATOR_H_

#include <base/capability.h>
#include <base/quota_guard.h>
#include <base/cache.h>
#include <dataspace/dataspace.h>

namespace Genode {

	struct Ram_dataspace : Dataspace { };

	typedef Capability<Ram_dataspace> Ram_dataspace_capability;

	struct Ram_allocator;

	class Constrained_ram_allocator;
}


struct Genode::Ram_allocator : Interface
{
	/**
	 * Allocate RAM dataspace
	 *
	 * \param  size    size of RAM dataspace
	 * \param  cached  selects cacheability attributes of the memory,
	 *                 uncached memory, i.e., for DMA buffers
	 *
	 * \throw  Out_of_ram
	 * \throw  Out_of_caps
	 *
	 * \return capability to new RAM dataspace
	 */
	virtual Ram_dataspace_capability alloc(size_t size,
	                                       Cache_attribute cached = CACHED) = 0;

	/**
	 * Free RAM dataspace
	 *
	 * \param ds  dataspace capability as returned by alloc
	 */
	virtual void free(Ram_dataspace_capability ds) = 0;

	/**
	 * Return size of dataspace in bytes
	 */
	virtual size_t dataspace_size(Ram_dataspace_capability ds) const = 0;
};


/**
 * Quota-bounds-checking wrapper of the 'Ram_allocator' interface
 */
class Genode::Constrained_ram_allocator : public Ram_allocator
{
	private:

		Ram_allocator   &_ram_alloc;
		Ram_quota_guard &_ram_guard;
		Cap_quota_guard &_cap_guard;

	public:

		Constrained_ram_allocator(Ram_allocator   &ram_alloc,
		                          Ram_quota_guard &ram_guard,
		                          Cap_quota_guard &cap_guard)
		:
			_ram_alloc(ram_alloc), _ram_guard(ram_guard), _cap_guard(cap_guard)
		{ }

		Ram_dataspace_capability alloc(size_t size, Cache_attribute cached) override
		{
			size_t page_aligned_size = align_addr(size, 12);

			Ram_quota_guard::Reservation ram (_ram_guard, Ram_quota{page_aligned_size});
			Cap_quota_guard::Reservation caps(_cap_guard, Cap_quota{1});

			/*
			 * \throw Out_of_caps, Out_of_ram
			 */
			Ram_dataspace_capability ds = _ram_alloc.alloc(page_aligned_size, cached);

			ram. acknowledge();
			caps.acknowledge();

			return ds;
		}

		void free(Ram_dataspace_capability ds) override
		{
			size_t const size = _ram_alloc.dataspace_size(ds);

			_ram_alloc.free(ds);

			_ram_guard.replenish(Ram_quota{size});
			_cap_guard.replenish(Cap_quota{1});
		}

		size_t dataspace_size(Ram_dataspace_capability ds) const override
		{
			return _ram_alloc.dataspace_size(ds);
		}
};

#endif /* _INCLUDE__BASE__RAM_ALLOCATOR_H_ */
